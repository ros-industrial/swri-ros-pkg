﻿/*
 * Software License Agreement (BSD License)
 *
 * Copyright (c) 2013, Southwest Research Institute
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *       * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *       * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *       * Neither the name of the Southwest Research Institute, nor the names
 *       of its contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
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

#ifndef FS100_JOINT_TRAJECTORY_STREAMER_H
#define FS100_JOINT_TRAJECTORY_STREAMER_H

#include "fs100/industrial_robot_client/joint_trajectory_streamer.h"
#include "fs100/simple_message/motoman_motion_ctrl.h"
#include "fs100/simple_message/motoman_motion_reply.h"
#include "simple_message/simple_message.h"

namespace motoman
{
namespace fs100_joint_trajectory_streamer
{

using industrial_robot_client::joint_trajectory_streamer::JointTrajectoryStreamer;
using industrial::simple_message::SimpleMessage;
using industrial::smpl_msg_connection::SmplMsgConnection;
using motoman::simple_message::motion_reply::MotionReply;
typedef motoman::simple_message::motion_ctrl::MotionControlCmd MotionControlCmd;

/**
 * \brief Message handler that streams joint trajectories to the robot controller.
 *        Contains FS100-specific motion control commands.
 */

//* JointTrajectoryStreamer
/**
 *
 * THIS CLASS IS NOT THREAD-SAFE
 *
 */
class FS100_JointTrajectoryStreamer : public JointTrajectoryStreamer
{

public:

  // since this class defines a different init(), this helps find the base-class init()
  using JointTrajectoryStreamer::init;

  /**
   * \brief Default constructor
   *
   * \param min_buffer_size minimum number of points as required by robot implementation
   */
  FS100_JointTrajectoryStreamer(int robot_id=0) : JointTrajectoryStreamer(1),
                                                  robot_id_(robot_id) {};

  ~FS100_JointTrajectoryStreamer();

  /**
   * \brief Class initializer
   *
   * \param connection simple message connection that will be used to send commands to robot (ALREADY INITIALIZED)
   * \param joint_names list of expected joint-names.
   *   - Count and order should match data sent to robot connection.
   *   - Use blank-name to insert a placeholder joint position (typ. 0.0).
   *   - Joints in the incoming JointTrajectory stream that are NOT listed here will be ignored.
   * \param velocity_limits map of maximum velocities for each joint
   *   - leave empty to lookup from URDF
   * \return true on success, false otherwise (an invalid message type)
   */
  virtual bool init(SmplMsgConnection* connection, const std::vector<std::string> &joint_names,
                    const std::map<std::string, double> &velocity_limits = std::map<std::string, double>());

  virtual bool send_to_robot(const std::vector<SimpleMessage>& messages);

protected:
  int robot_id_;

  void trajectoryStop();

  bool sendMotionCtrlMsg(MotionControlCmd command, MotionReply &reply);
  bool controllerReady();
  bool setTrajMode(bool enable);
  static std::string getErrorString(const MotionReply &reply);
};

} //fs100_joint_trajectory_streamer
} //industrial_robot_client

#endif /* FS100_JOINT_TRAJECTORY_STREAMER_H */