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

#ifndef __definitions_h
#define __definitions_h

// Definitions
#define MOTION_PORT 11000
#define SYSTEM_PORT 11001
#define BUFF_MAX 1023
#define QSIZE 20
#define JOINTSPEED 500 // 1000 = 10% motor speed
#define JOBNAME "PVARQ20"

// Commands
#define UNUSED 0
#define CMD_INIT_PVQ 1
#define CMD_ADD_POINT_PVQ 2
#define CMD_END_PVQ 3
#define CMD_HOLD 4
#define CMD_GET_FB_PULSE 5
#define CMD_GET_FB_SPEED 6
#define CMD_GET_TORQUE 7

// Reply Codes
#define RC_SUCCESS 1
#define RC_MP_ERROR 2
#define RC_AE 3 // already exists
#define RC_NOT_INIT 4
#define RC_MOTION_INTERRUPT 5

#endif