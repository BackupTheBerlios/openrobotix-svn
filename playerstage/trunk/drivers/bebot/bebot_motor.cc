/*
 *  bebot_motor.cc - Player driver for BeBot-Robot motor controller
 *  Copyright (C) 2007 - 2008
 *    Heinz Nixdorf Institute - University of Paderborn
 *    Department of System and Circuit Technology
 *    Alwin Heerklotz
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
/** @defgroup driver_bebot_motor bebot_motor
 * @brief BeBot motor controller

The bebot_motor driver controls the motors of the BeBot.

@par Compile-time dependencies

- none

@par Provides

- @ref position2d

@par Configuration requests

- PLAYER_POSITION2D_CMD_VEL
- PLAYER_POSITION2D_REQ_GET_GEOM

@par Configuration file options

- sleep_nsec (integer)
  - Default: 100000000 (=100ms)
  - timespec value for nanosleep()

@author Alwin Heerklotz Stefan Herbrechtsmeier

*/
/** @} */

#include <unistd.h>
#include <string.h>
#include  <fcntl.h>

#include <libplayercore/playercore.h>

#define WIDTH 0.09
#define LENGTH 0.09

// The class for the driver
class BeBotMotor : public Driver
{
  // Constructor
  public: BeBotMotor(ConfigFile* cf, int section);

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
  private: int device_file;
  private: int sleep_nsec;

  // Bugfix : terminate called without an active exception
  private: int thread_run;
};

// Initialization function.
Driver* BeBotMotor_Init(ConfigFile* cf, int section)
{
  // Create and return a new instance of this driver
  return((Driver*)(new BeBotMotor(cf, section)));
}

// Driver registration function
void BeBotMotor_Register(DriverTable* table)
{
  table->AddDriver("bebotmotor", BeBotMotor_Init);
}

// Constructor
BeBotMotor::BeBotMotor(ConfigFile* cf, int section)
    : Driver(cf,
             section,
             false,
             PLAYER_MSGQUEUE_DEFAULT_MAXLEN,
             PLAYER_POSITION2D_CODE)
{
  this->device_name = cf->ReadString(section, "device", "/dev/motor0");

  this->sleep_nsec = cf->ReadInt(section, "sleep_nsec", 100000000);
}

// Set up the device.  Return 0 if things go well, and -1 otherwise.
int BeBotMotor::Setup()
{
  // Start the device thread; spawns a new thread and executes
  this->thread_run = 1;
  StartThread();

  return 0;
}

// Shutdown the device
int BeBotMotor::Shutdown()
{
  // Stop and join the driver thread
  this->thread_run = 0;
//  StopThread();

  return 0;
}

int BeBotMotor::ProcessMessage(QueuePointer & resp_queue,
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

    //PLAYER_MSG2(2,"sending motor commands for px=%f, pa=%f",
    //            position_cmd.vel.px,
    //            position_cmd.vel.pa);

    if (this->setSpeeds(position_cmd.vel.px, position_cmd.vel.pa))
    {
      PLAYER_ERROR("failed to write speeds to device");
    }
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

    geom.size.sl = LENGTH;
    geom.size.sw = WIDTH;

    this->Publish(device_addr, resp_queue,
                  PLAYER_MSGTYPE_RESP_ACK,
                  PLAYER_POSITION2D_REQ_GET_GEOM,
                  (void*)&geom, sizeof(geom), NULL);
    return 0;
  }

  return -1;
}

int BeBotMotor::setSpeeds(float v_translate, float v_rotate)
{
  float v_left = v_translate - v_rotate * WIDTH / 2.0;  // m/s
  float v_right = v_translate + v_rotate * WIDTH / 2.0; // m/s

  signed short buf[2];
  buf[0] = (short) ((float) v_left * 1000); // mm/s;
  buf[1] = (short) ((float) v_right * 1000); // mm/s;

  this->device_file = open(this->device_name, O_RDWR);
  int result = write(this->device_file, (void*)&buf, 4);
  close(this->device_file);

  if(result != 4)
    return -1;

  return 0;
}

// Main function for device thread
void BeBotMotor::Main() 
{
//  int oldstate;
  struct timespec tspec;

  // The main loop; interact with the device here
  while(this->thread_run)
  {
    // Test if we are supposed to cancel this thread.
    pthread_testcancel();

    // Go to sleep for a while.
    tspec.tv_sec = 0;
    tspec.tv_nsec = this->sleep_nsec;
    nanosleep(&tspec, NULL);

    // Process incoming messages.  HNIMotorDriver::ProcessMessage() is
    // called on each message.
    ProcessMessages();

    // Interact with the device, and push out the resulting data, using
    // Driver::Publish()
#if 0
//    pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &oldstate);

    short buf[2];

    // read speed for left and right motor in mm/s
    read(this->device_file,(void*)&buf, 4);

    double v_left = buf[0] / 1000.0; // m/s
    double v_right = buf[1] / 1000.0; // m/s

    player_position2d_data_t position2d_data;
    position2d_data.pos.px = 0; // TODO
    position2d_data.pos.py = 0;
    position2d_data.pos.pa = 0; // TODO
    position2d_data.vel.px = (v_left + v_right)/2.0; // m/s
    position2d_data.vel.py = 0; // robot is not able to move sideward
    position2d_data.vel.pa = (buf[0] - buf[1])/WIDTH; // rad/s
    position2d_data.stall = 0; // TODO: information is missing at /dev/motor0
    this->Publish(device_addr,
                  PLAYER_MSGTYPE_DATA,
                  PLAYER_POSITION2D_DATA_STATE,
                  (void*) &position2d_data,
                  sizeof(position2d_data),
                  NULL);

//    pthread_setcancelstate(oldstate, NULL);
#endif
  }
}

// Extra stuff for building a shared object.

/* need the extern to avoid C++ name-mangling  */
extern "C" {
  int player_driver_init(DriverTable* table)
  {
    puts("BeBotMotor driver initializing");
    BeBotMotor_Register(table);
    puts("BeBotMotor driver done");
    return 0;
  }
}
