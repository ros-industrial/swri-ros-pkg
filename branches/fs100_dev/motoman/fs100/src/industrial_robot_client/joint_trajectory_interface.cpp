/*
 * Software License Agreement (BSD License)
 *
 * Copyright (c) 2013, Southwest Research Institute
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 	* Redistributions of source code must retain the above copyright
 * 	notice, this list of conditions and the following disclaimer.
 * 	* Redistributions in binary form must reproduce the above copyright
 * 	notice, this list of conditions and the following disclaimer in the
 * 	documentation and/or other materials provided with the distribution.
 * 	* Neither the name of the Southwest Research Institute, nor the names
 *	of its contributors may be used to endorse or promote products derived
 *	from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <algorithm>
#include "fs100/industrial_robot_client/joint_trajectory_interface.h"
#include "simple_message/joint_traj_pt.h"
#include "industrial_utils/param_utils.h"

using namespace industrial_utils::param;
using industrial::simple_message::SimpleMessage;
namespace SpecialSeqValues = industrial::joint_traj_pt::SpecialSeqValues;
typedef industrial::joint_traj_pt::JointTrajPt rbt_JointTrajPt;
typedef trajectory_msgs::JointTrajectoryPoint  ros_JointTrajPt;

namespace industrial_robot_client
{
namespace joint_trajectory_interface
{

bool JointTrajectoryInterface::init(std::string default_ip, int default_port)
{
  std::string ip;
  int port;

  // override IP/port with ROS params, if available
  ros::param::param<std::string>("robot_ip_address", ip, default_ip);
  ros::param::param<int>("~port", port, default_port);

  // check for valid parameter values
  if (ip.empty())
  {
    ROS_ERROR("No valid robot IP address found.  Please set ROS 'robot_ip_address' param");
    return false;
  }
  if (port <= 0)
  {
    ROS_ERROR("No valid robot IP port found.  Please set ROS '~port' param");
    return false;
  }

  char* ip_addr = strdup(ip.c_str()); // connection.init() requires "char*", not "const char*"
  ROS_INFO("Joint Trajectory Interface connecting to IP address: '%s:%d'", ip_addr, port);
  default_tcp_connection_.init(ip_addr, port);
  free(ip_addr);

  return init(&default_tcp_connection_);
}

bool JointTrajectoryInterface::init(SmplMsgConnection* connection)
{
  std::vector<std::string> joint_names;
  if (!getJointNames("controller_joint_names", "robot_description", joint_names))
    ROS_WARN("Unable to read 'controller_joint_names' param.  Using standard 6-DOF joint names.");

  return init(connection, joint_names);
}

bool JointTrajectoryInterface::init(SmplMsgConnection* connection, const std::vector<std::string> &joint_names,
                                    const std::map<std::string, double> &velocity_limits)
{
  this->connection_ = connection;
  this->all_joint_names_ = joint_names;
  this->joint_vel_limits_ = velocity_limits;
  connection_->makeConnect();

  this->srv_stop_motion_ = this->node_.advertiseService("stop_motion", &JointTrajectoryInterface::stopMotionCB, this);
  this->srv_joint_trajectory_ = this->node_.advertiseService("joint_path_command", &JointTrajectoryInterface::jointTrajectoryCB, this);
  this->sub_joint_trajectory_ = this->node_.subscribe("joint_path_command", 0, &JointTrajectoryInterface::jointTrajectoryCB, this);

  return true;
}

JointTrajectoryInterface::~JointTrajectoryInterface()
{  
  trajectoryStop();
  this->sub_joint_trajectory_.shutdown();
}

bool JointTrajectoryInterface::jointTrajectoryCB(industrial_msgs::CmdJointTrajectory::Request &req,
                                                 industrial_msgs::CmdJointTrajectory::Response &res)
{
  trajectory_msgs::JointTrajectoryPtr traj_ptr(new trajectory_msgs::JointTrajectory);
  *traj_ptr = req.trajectory;  // copy message data
  this->jointTrajectoryCB(traj_ptr);

  // no success/fail result from jointTrajectoryCB.  Assume success.
  res.code.val = industrial_msgs::ServiceReturnCode::SUCCESS;

  return true;  // always return true.  To distinguish between call-failed and service-unavailable.
}

void JointTrajectoryInterface::jointTrajectoryCB(const trajectory_msgs::JointTrajectoryConstPtr &msg)
{
  ROS_INFO("Receiving joint trajectory message");

  // check for STOP command
  if (msg->points.empty())
  {
    ROS_INFO("Empty trajectory received, canceling current trajectory");
    trajectoryStop();
    return;
  }

  // convert trajectory into robot-format
  std::vector<SimpleMessage> robot_msgs;
  if (!trajectory_to_msgs(msg, &robot_msgs))
    return;

  // send command messages to robot
  send_to_robot(robot_msgs);
}

bool JointTrajectoryInterface::trajectory_to_msgs(const trajectory_msgs::JointTrajectoryConstPtr& traj, std::vector<SimpleMessage>* msgs)
{
  msgs->clear();

  for (size_t i=0; i<traj->points.size(); ++i)
  {
    SimpleMessage msg;
    ros_JointTrajPt rbt_pt, xform_pt;

    // select / reorder joints for sending to robot
    if (!select(traj->joint_names, traj->points[i], this->all_joint_names_, &rbt_pt))
      return false;

    // transform point data (e.g. for joint-coupling)
    if (!transform(rbt_pt, &xform_pt))
      return false;

    // convert trajectory point to ROS message
    if (!create_message(i, xform_pt, &msg))
      return false;

    msgs->push_back(msg);
  }

  return true;
}

bool JointTrajectoryInterface::select(const std::vector<std::string>& ros_joint_names, const ros_JointTrajPt& ros_pt,
                      const std::vector<std::string>& rbt_joint_names, ros_JointTrajPt* rbt_pt)
{
  ROS_ASSERT(ros_joint_names.size() == ros_pt.positions.size());

  // initialize rbt_pt
  *rbt_pt = ros_pt;
  rbt_pt->positions.clear(); rbt_pt->velocities.clear(); rbt_pt->accelerations.clear();

  for (size_t rbt_idx=0; rbt_idx < rbt_joint_names.size(); ++rbt_idx)
  {
    bool is_empty = rbt_joint_names[rbt_idx].empty();

    // find matching ROS element
    size_t ros_idx = std::find(ros_joint_names.begin(), ros_joint_names.end(), rbt_joint_names[rbt_idx]) - ros_joint_names.begin();
    bool is_found = ros_idx < ros_joint_names.size();

    // error-chk: required robot joint not found in ROS joint-list
    if (!is_empty && !is_found)
    {
      ROS_ERROR("Expected joint (%s) not found in JointTrajectory.  Aborting command.", rbt_joint_names[rbt_idx].c_str());
      return false;
    }

    if (is_empty)
    {
      if (!ros_pt.positions.empty()) rbt_pt->positions.push_back(default_joint_pos_);
      if (!ros_pt.velocities.empty()) rbt_pt->velocities.push_back(-1);
      if (!ros_pt.accelerations.empty()) rbt_pt->accelerations.push_back(-1);
    }
    else
    {
      if (!ros_pt.positions.empty()) rbt_pt->positions.push_back(ros_pt.positions[ros_idx]);
      if (!ros_pt.velocities.empty()) rbt_pt->velocities.push_back(ros_pt.velocities[ros_idx]);
      if (!ros_pt.accelerations.empty()) rbt_pt->accelerations.push_back(ros_pt.accelerations[ros_idx]);
    }
  }
  return true;
}

// default velocity calculation computes the %-of-max-velocity for the "critical joint" (closest to velocity-limit)
// such that 0.2 = 20% of maximum joint speed.
//
// NOTE: this calculation uses the maximum joint speeds from the URDF file, which may differ from those defined on
// the physical robot.  These differences could lead to different actual movement velocities than intended.
// Behavior should be verified on a physical robot if movement velocity is critical.
bool JointTrajectoryInterface::calc_velocity(const trajectory_msgs::JointTrajectoryPoint& pt, double* rbt_velocity)
{
  std::vector<double> vel_ratios;
  static bool limits_initialized = false;

  ROS_ASSERT(all_joint_names_.size() == pt.positions.size());

  // check for empty velocities in ROS topic
  if (pt.velocities.empty())
  {
    ROS_WARN("Joint velocities unspecified.  Using default/safe speed.");
    *rbt_velocity = default_vel_ratio_;
    return true;
  }

  // read velocity limits from URDF if not specified
  if (joint_vel_limits_.empty() && !limits_initialized)
  {
    limits_initialized = true;
    if (!getJointVelocityLimits("robot_description", joint_vel_limits_))
      ROS_WARN("Unable to read velocity limits from 'robot_description' param.  Will use default/safe speed.");
  }

  for (size_t i=0; i<all_joint_names_.size(); ++i)
  {
    const std::string &jnt_name = all_joint_names_[i];

    // update vel_ratios
    if (jnt_name.empty())                             // ignore "dummy joints" in velocity calcs
      vel_ratios.push_back(-1);
    else if (joint_vel_limits_.count(jnt_name) == 0)  // no velocity limit specified for this joint
      vel_ratios.push_back(-1);
    else
      vel_ratios.push_back( fabs(pt.velocities[i] / joint_vel_limits_[jnt_name]) );  // calculate expected duration for this joint
  }

  // find largest velocity-ratio (closest to max joint-speed)
  int max_idx = std::max_element(vel_ratios.begin(), vel_ratios.end()) - vel_ratios.begin();
  
  if (vel_ratios[max_idx] > 0)
    *rbt_velocity = vel_ratios[max_idx];
  else
  {
    ROS_WARN("Joint velocity-limits unspecified.  Using default velocity-ratio.");
    *rbt_velocity = default_vel_ratio_;
  }

  if ( (*rbt_velocity < 0) || (*rbt_velocity > 1) )
  {
    ROS_WARN("computed velocity (%.1f %%) is out-of-range.  Clipping to [0-100%%]", *rbt_velocity * 100);
    *rbt_velocity = std::min(1.0, std::max(0.0, *rbt_velocity));  // clip to [0,1]
  }
  
  return true;
}

bool JointTrajectoryInterface::calc_duration(const trajectory_msgs::JointTrajectoryPoint& pt, double* rbt_duration)
{
  std::vector<double> durations;
  double this_time = pt.time_from_start.toSec();
  static double last_time = 0;

  if (this_time <= last_time)  // earlier time => new trajectory.  Move slowly to first point.
    *rbt_duration = default_duration_;
  else
    *rbt_duration = this_time - last_time;

  last_time = this_time;

  return true;
}

bool JointTrajectoryInterface::create_message(int seq, const trajectory_msgs::JointTrajectoryPoint &pt, SimpleMessage *msg)
{
  industrial::joint_data::JointData pos;
  ROS_ASSERT(pt.positions.size() <= (unsigned int)pos.getMaxNumJoints());

  for (size_t i=0; i<pt.positions.size(); ++i)
    pos.setJoint(i, pt.positions[i]);

  // calculate velocity & duration
  double velocity, duration;
  if (!calc_velocity(pt, &velocity) || !calc_duration(pt, &duration))
    return false;

  rbt_JointTrajPt msg_data;
  msg_data.init(seq, pos, velocity, duration);

  JointTrajPtMessage jtp_msg;
  jtp_msg.init(msg_data);

  return jtp_msg.toTopic(*msg);  // assume "topic" COMM_TYPE for now
}

void JointTrajectoryInterface::trajectoryStop()
{
  JointTrajPtMessage jMsg;
  SimpleMessage msg, reply;

  ROS_INFO("Joint trajectory handler: entering stopping state");
  jMsg.setSequence(SpecialSeqValues::STOP_TRAJECTORY);
  jMsg.toRequest(msg);
  ROS_DEBUG("Sending stop command");
  this->connection_->sendAndReceiveMsg(msg, reply);
}

bool JointTrajectoryInterface::stopMotionCB(industrial_msgs::StopMotion::Request &req,
                                            industrial_msgs::StopMotion::Response &res)
{
  trajectoryStop();

  // no success/fail result from trajectoryStop.  Assume success.
  res.code.val = industrial_msgs::ServiceReturnCode::SUCCESS;

  return true;  // always return true.  To distinguish between call-failed and service-unavailable.
}

} //joint_trajectory_interface
} //industrial_robot_client

