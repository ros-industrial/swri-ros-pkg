/*
 * Software License Agreement (BSD License)
 *
 * Copyright (c) 2011, Yaskawa America, Inc.
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
 *       * Neither the name of the Yaskawa America, Inc., nor the names
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

#include "joint_trajectory_downloader.h"
#include "simple_message/socket/simple_socket.h"
#include "simple_message/socket/tcp_client.h"
#include "industrial_utils/param_utils.h"

using namespace industrial::simple_socket;
using namespace industrial_robot_client::joint_trajectory_downloader;

int main(int argc, char** argv)
{
  ros::init(argc, argv, "joint_trajectory_handler");
  ros::NodeHandle node;
  std::string s;
  std::vector<std::string> joint_names;

  if (node.getParam("robot_ip_address", s))
  {
    char* ip_addr = strdup(s.c_str());
    ROS_INFO("Motion download interface connecting to IP address: %s", ip_addr);

    if (industrial_utils::param::getListParam("controller_joint_names", joint_names))
    {
      industrial::tcp_client::TcpClient robot;
      robot.init(ip_addr, StandardSocketPorts::MOTION);
      JointTrajectoryDownloader jtHandler(node, &robot, joint_names);

      free(ip_addr);

      ros::spin();
    }
    else
    {
      ROS_FATAL("Failed to get param 'controller_joint_name'");
    }
  }
  else
  {
    ROS_FATAL("Failed to get param 'robot_ip_address'");
  }

  return 0;
}
