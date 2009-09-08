/*
 *  bebot_ir.cc - Player driver for BeBot-Robot ir sensors
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
 * This driver provides an ir interface for a BeBot-robot by using senseact
 * device files.
 */

/** @ingroup drivers */
/** @{ */
/** @defgroup driver_bebot_ir bebot_ir
 * @brief BeBot ir array

The bebot_ir driver controls the ir sensors of the BeBot.

@par Compile-time dependencies

- senseact

@par Provides

- @ref interface_ir

@par Configuration requests

- PLAYER_IR_REQ_POSE

@par Configuration file options

- devices (string)
  - Default: /dev/senseact0
  - senseact BeBot ir sensor devices

- counts (integer)
  - Default: 1
  - Number of sensors per device

- poses (string)
  - Default: 0 0 0 0 0 0
  - sensor positions

@author Stefan Herbrechtsmeier

*/
/** @} */

#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>

#include <libplayercore/playercore.h>
#include <linux/senseact.h>

// The class for the driver
class BeBotIR : public Driver
{
  // Constructor
  public: BeBotIR(ConfigFile* cf, int section);
  public: ~BeBotIR();

  // Setup/shutdown routines.
  public: virtual int Setup();
  public: virtual int Shutdown();

  // This method will be invoked on each incoming message
  public: virtual int ProcessMessage(QueuePointer &resp_queue,
                               player_msghdr * hdr,
                               void * data);

  // Main function for device thread.
  private: virtual void Main();

  private: int devices_count;
  private: int *devices;
  private: int devices_nfds;
  private: const char **devices_name;
  private: int *sensors_count;
  private: int sensors_sum;
  private: player_pose3d_t *positions;

  // Bugfix : terminate called without an active exception
//  private: int thread_run;
};

// Initialization function.
Driver* BeBotIR_Init(ConfigFile* cf, int section)
{
  // Create and return a new instance of this driver
  return((Driver*)(new BeBotIR(cf, section)));
}

// Driver registration function
void BeBotIR_Register(DriverTable* table)
{
  table->AddDriver("bebotir", BeBotIR_Init);
}

// Constructor
BeBotIR::BeBotIR(ConfigFile* cf, int section)
    : Driver(cf,
             section,
             false,
             PLAYER_MSGQUEUE_DEFAULT_MAXLEN,
             PLAYER_IR_CODE)
{
  int tuple_count;

  tuple_count = cf->GetTupleCount(section, "devices");

  if (tuple_count == 0)
    this->devices_count = 1;
  else
    this->devices_count = tuple_count;

  this->devices = new int[this->devices_count];
  this->devices_name = new const char*[this->devices_count];

  for (int i = 0 ; i < this->devices_count; i++) 
  {
    this->devices_name[i] =cf->ReadTupleString(section, "devices", i,
                                                "/dev/senseact0");
  }

  this->sensors_sum = 0;

  this->sensors_count = new int[this->devices_count];
  for (int i = 0 ; i < this->devices_count; i++)
  {
    this->sensors_count[i] = cf->ReadTupleInt(section, "counts", i, 1);
    sensors_sum += this->sensors_count[i];
  }

  this->positions = new player_pose3d_t[this->sensors_sum];

  for (int i = 0 ; i < this->sensors_sum; i++)
  {
    positions[i].px = cf->ReadTupleFloat(section, "positions", i*6, 0);
    positions[i].py = cf->ReadTupleFloat(section, "positions", i*6+1, 0);
    positions[i].pz = cf->ReadTupleFloat(section, "positions", i*6+2, 0);
    positions[i].proll = cf->ReadTupleFloat(section, "positions", i*6+3, 0);
    positions[i].ppitch = cf->ReadTupleFloat(section, "positions", i*6+4, 0);
    positions[i].pyaw = cf->ReadTupleFloat(section, "positions", i*6+5, 0);
  }
}

// Destructor.  Destroy any created objects.
BeBotIR::~BeBotIR()
{
  delete [] this->devices;
  delete [] this->devices_name;
  delete [] this->sensors_count;
  delete [] this->positions;
}

// Set up the device.  Return 0 if things go well, and -1 otherwise.
int BeBotIR::Setup()
{
  this->devices_nfds = 0;
  for (int i = 0; i < this->devices_count; i++)
  {
    this->devices[i] = open(this->devices_name[i], O_RDONLY | O_NONBLOCK);
    if (this->devices[i] == -1)
    {
      PLAYER_ERROR1("Couldn't open senseact device %s", this->devices_name[i]);
      for (int j = 0; j < i; j++)
	close(this->devices[j]);
      return -1;
    }
    if (this->devices[i] > this->devices_nfds)
      this->devices_nfds = this->devices[i];
  }
  
//  this->thread_run = 1;
  this->StartThread();

  return 0;
}

// Shutdown the device (called by server thread).
int BeBotIR::Shutdown()
{
//  this->thread_run = 0;
  StopThread();

  for (int i = 0; i < this->devices_count; i++)
    close(this->devices[i]);

  return 0;
}

int BeBotIR::ProcessMessage(QueuePointer & resp_queue,
                            player_msghdr * hdr,
                            void * data)
{
  assert (hdr);

  // Process messages here.  Send a response if necessary, using Publish().
  if(Message::MatchMessage(hdr,
                           PLAYER_MSGTYPE_REQ,
                           PLAYER_IR_REQ_POSE,
                           device_addr))
  {
    /* Return the sensor poses. */
    player_ir_pose_t pose;

    pose.poses_count = this->sensors_sum;
    pose.poses = this->positions;

    this->Publish(device_addr, resp_queue,
                  PLAYER_MSGTYPE_RESP_ACK,
                  PLAYER_IR_REQ_POSE,
                  (void*)&pose, sizeof(pose), NULL);
    return 0;
  }

  return -1;
}

// Main function for device thread
void BeBotIR::Main() 
{
  int values[this->sensors_sum];
  float voltages[this->sensors_sum];
  float ranges[this->sensors_sum];

//  while(this->thread_run)
  while(1)
  {
    pthread_testcancel();

    ProcessMessages();

//    pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &oldstate);

    int publish = 0;

    fd_set rfds;
    
    FD_ZERO(&rfds);
    for (int i = 0; i < this->devices_count; i++)
      FD_SET(this->devices[i], &rfds);

    int rv = select(this->devices_nfds + 1, &rfds, NULL, NULL, NULL);

    if (rv == -1) // error
      break;
    else if (rv) // data available
    {
      int offset = 0;
      for (int i = 0; i < this->devices_count; i++)
      {
	if (FD_ISSET(this->devices[i], &rfds))
	{
	  struct senseact_action actions[this->sensors_count[i] + 1];
	  int n = read(this->devices[i], (void*)actions,
		   (this->sensors_count[i] + 1) * sizeof(struct senseact_action));

	  for (int j = 0; j < n; j++)
	  {
	    if (actions[j].type == SENSEACT_TYPE_BRIGHTNESS)
	    {
	      if ((actions[j].index) < this->sensors_count[i])
	        values[offset + actions[j].index] = actions[j].value;
	    }
	    else if (actions[j].type == SENSEACT_TYPE_SYNC &&
		     actions[j].index == SENSEACT_SYNC_SENSOR)
	    {
	      for (int k = offset; k < (offset + this->sensors_count[i]); k++)
	      {
		voltages[k] = values[k] * 0.001; // voltage in V
		if (voltages[k] > 0)
		  ranges[k] = (1.0 / voltages[k] + 4.0)  * 0.01;
		if (ranges[k] < 0)
		  ranges[k] = 0;
		if (ranges[k] > 0.14)
		  ranges[k] = 0.14;
	      }

	      publish = 1;
	    }
	  }
	}
	offset += this->sensors_count[i];
      }

      if (publish)
      {
	player_ir_data_t ir_data;
	ir_data.voltages_count = this->sensors_sum;
	ir_data.voltages = voltages;
	ir_data.ranges_count = this->sensors_sum;
	ir_data.ranges = ranges;

	this->Publish(device_addr,
		      PLAYER_MSGTYPE_DATA,
		      PLAYER_IR_DATA_RANGES,
		      (void*) &ir_data,
		      sizeof(ir_data),
		      NULL);
      }
    }
//  pthread_setcancelstate(oldstate, NULL);
  }
}

// Extra stuff for building a shared object.

/* need the extern to avoid C++ name-mangling  */
extern "C" {
  int player_driver_init(DriverTable* table)
  {
    puts("BeBotIR driver initializing");
    BeBotIR_Register(table);
    puts("BeBotIR driver done");
    return 0;
  }
}
