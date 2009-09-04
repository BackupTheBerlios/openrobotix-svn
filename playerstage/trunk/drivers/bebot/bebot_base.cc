/*
 *  bebot_base.cc - Player driver for BeBot-Robot motor controller
 *  Copyright (C) 2007 - 2009
 *    Heinz Nixdorf Institute - University of Paderborn
 *    Department of System and Circuit Technology
 *    Stefan Herbrechtsmeier <hbmeier@hni.uni-paderborn.de>
 *  based on exampledriver.cc by Brian Gerkey
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

/*
 * This driver provides a position2d interface for a BeBot-robot by using motor
 * device files.
 */

/** @ingroup drivers */
/** @{ */
/** @defgroup driver_bebot_base bebot_base
 * @brief BeBot base controller

The bebot_base controls the motors of the BeBot.

@par Compile-time dependencies

- senseact

@par Provides

- @ref position2d

@par Configuration requests

- PLAYER_POSITION2D_CMD_VEL
- PLAYER_POSITION2D_REQ_GET_GEOM

@par Configuration file options

- device (string)
  - Default: /dev/senseact0
  - senseact BeBot base device

- position
  - Default: [0 0 0]
  - Offset position

@author Stefan Herbrechtsmeier

*/
/** @} */

#include <unistd.h>
#include <string.h>
#include <fcntl.h>

#include <libplayercore/playercore.h>
#include <linux/senseact.h>

#define WIDTH 90
#define LENGTH 90

// The class for the driver
class BeBotBase : public Driver
{
  // Constructor
  public: BeBotBase(ConfigFile* cf, int section);

  // Setup/shutdown routines.
  public: virtual int Setup();
  public: virtual int Shutdown();

  // This method will be invoked on each incoming message
  public: virtual int ProcessMessage(QueuePointer &resp_queue,
                               player_msghdr * hdr,
                               void * data);

  // Main function for device thread.
  private: virtual void Main();

  private: int setSpeeds(float v_translate, float v_rotate);

  private: const char* device_name;
  private: player_pose2d_t position;
  private: int device;

  // Bugfix : terminate called without an active exception
//  private: int thread_run;
};

// Initialization function.
Driver* BeBotBase_Init(ConfigFile* cf, int section)
{
  // Create and return a new instance of this driver
  return((Driver*)(new BeBotBase(cf, section)));
}

// Driver registration function
void BeBotBase_Register(DriverTable* table)
{
  table->AddDriver("bebotbase", BeBotBase_Init);
}

// Constructor
BeBotBase::BeBotBase(ConfigFile* cf, int section)
    : Driver(cf,
             section,
             false,
             PLAYER_MSGQUEUE_DEFAULT_MAXLEN,
             PLAYER_POSITION2D_CODE)
{
  this->device_name = cf->ReadString(section, "device", "/dev/senseact0");

  this->position.px = cf->ReadTupleFloat(section, "position", 0, 0);
  this->position.py = cf->ReadTupleFloat(section, "position", 1, 0);
  this->position.pa = cf->ReadTupleFloat(section, "position", 2, 0);

}

// Set up the device.  Return 0 if things go well, and -1 otherwise.
int BeBotBase::Setup()
{
  this->device = open(this->device_name, O_RDONLY);
  if (this->device == -1)
  {
    PLAYER_ERROR1("Couldn't open senseact device %s", this->device);
    return -1;
  }

//  this->thread_run = 1;
  StartThread();

  return 0;
}

// Shutdown the device
int BeBotBase::Shutdown()
{
//  this->thread_run = 0;
  StopThread();

  close(this->device);

  return 0;
}

int BeBotBase::ProcessMessage(QueuePointer & resp_queue,
                                   player_msghdr * hdr,
                                   void * data)
{
  assert (hdr);

  // Process messages here.  Send a response if necessary, using Publish().
  if(Message::MatchMessage(hdr,
                           PLAYER_MSGTYPE_CMD,
                           PLAYER_POSITION2D_CMD_VEL,
                           device_addr))
  {
    assert(data);
    // get and send the latest motor command
    player_position2d_cmd_vel_t position_cmd;
    position_cmd = *(player_position2d_cmd_vel_t *) data;

    // m/s to mm/s
    int translation = position_cmd.vel.px * 1000.0;
    int rotation = position_cmd.vel.pa * 1000.0;
    
    struct senseact_action actions[2];

    // left
    actions[0].type = SENSEACT_TYPE_SPEED;
    actions[0].index = 0;
    actions[0].value = translation - rotation * WIDTH / 2;
    
    // right
    actions[1].type = SENSEACT_TYPE_SPEED;
    actions[1].index = 1;
    actions[1].value = translation + rotation * WIDTH / 2;
    
    int rc = write(this->device, (void*)actions,
		   2 * sizeof(struct senseact_action));

    if (rc != 2 * sizeof(struct senseact_action))
    {
      PLAYER_ERROR2("failed to write speeds to device %s - %i", this->device_name, rc);
      return -1;  
    }

    return 0;
  } else if (Message::MatchMessage(hdr,
				 PLAYER_MSGTYPE_REQ,
				 PLAYER_POSITION2D_REQ_MOTOR_POWER,
				 device_addr))
  {
    this->Publish(device_addr, resp_queue,
		  PLAYER_MSGTYPE_RESP_ACK,
		  PLAYER_POSITION2D_REQ_MOTOR_POWER);
    return 0;    
  } else if(Message::MatchMessage(hdr,
                                  PLAYER_MSGTYPE_REQ,
                                  PLAYER_POSITION2D_REQ_GET_GEOM,
                                  device_addr))
  {
    /* Return the robot geometry. */
    player_position2d_geom_t geom;

    // Assume that it turns about its geometric center
    geom.pose.px = 0.0;
    geom.pose.py = 0.0;
    geom.pose.pz = 0.0;
    geom.pose.proll = 0.0;
    geom.pose.ppitch = 0.0;
    geom.pose.pyaw = 0.0;

    geom.size.sl = LENGTH / 1000;
    geom.size.sw = WIDTH / 1000;

    this->Publish(device_addr, resp_queue,
                  PLAYER_MSGTYPE_RESP_ACK,
                  PLAYER_POSITION2D_REQ_GET_GEOM,
                  (void*)&geom, sizeof(geom), NULL);
    return 0;
  }

  return -1;
}

// Main function for device thread
void BeBotBase::Main() 
{
//  int oldstate;
  float position[2] = {0, 0};
  float angle = 0;
  float speed[2] = {0, 0};

//  while(this->thread_run)
  while(1)
  {
    pthread_testcancel();

    ProcessMessages();

//    pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &oldstate);

    struct senseact_action actions[10];

    int n = read(this->device, (void*)actions,
		 10 * sizeof(struct senseact_action));

    for (int i = 0; i < n; i++)
    {
      switch (actions[i].type)
      {
      case SENSEACT_TYPE_SPEED:
	if (actions[i].index > 1 &&
	    actions[i].index < 4)
	 speed[actions[i].index - 2] = actions[i].value;
	break;

      case SENSEACT_TYPE_POSITION:
	if (actions[i].index < 2)
	 position[actions[i].index] = actions[i].value;
	break;

      case SENSEACT_TYPE_ANGLE:
	if (actions[i].index < 1)
	 angle = actions[i].value;
	break;

      case SENSEACT_TYPE_SYNC:
	if (actions[i].index == SENSEACT_SYNC_SENSOR)
        {
	  player_position2d_data_t position2d_data;

	  position2d_data.pos.px = this->position.px + position[0] / 1000.0; // m/s
	  position2d_data.pos.py = this->position.py + position[1] / 1000.0; // m/s
	  position2d_data.pos.pa = this->position.pa + angle / 1000.0; // m/s
	  position2d_data.vel.px = (speed[0] + speed[1]) / 1000.0 / 2.0; // m/s
	  position2d_data.vel.py = 0;
	  position2d_data.vel.pa = (speed[0] - speed[1]) / 1000.0 / WIDTH / 1000.0 * 2; // rad/s
	  position2d_data.stall = 0;

	  this->Publish(device_addr,
			PLAYER_MSGTYPE_DATA,
			PLAYER_POSITION2D_DATA_STATE,
			(void*) &position2d_data,
			sizeof(position2d_data),
			NULL);
	}
	break;
      }
    }
//    pthread_setcancelstate(oldstate, NULL);
  }
}

// Extra stuff for building a shared object.

/* need the extern to avoid C++ name-mangling  */
extern "C" {
  int player_driver_init(DriverTable* table)
  {
    puts("BeBotBase driver initializing");
    BeBotBase_Register(table);
    puts("BeBotBase driver done");
    return 0;
  }
}
