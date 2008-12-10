/*
 *  bebot_ir.cc - Player driver for BeBot-Robot ir sensors
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
 * This driver provides an ir interface for a BeBot-robot by using irsensor
 * device files.
 */

/** @ingroup drivers */
/** @{ */
/** @defgroup driver_bebot_ir bebot_ir
 * @brief BeBot ir array

The bebot_ir driver controls the irs of the BeBot.

@par Compile-time dependencies

- none

@par Provides

- @ref interface_ir

@par Configuration requests

- PLAYER_IR_REQ_POSE

@par Configuration file options

- sleep_nsec (integer)
  - Default: 10000000 (=10ms)
  - timespec value for nanosleep()

@author Alwin Heerklotz Stefan Herbrechtsmeier

*/
/** @} */

#include <unistd.h>
#include <string.h>
#include <fcntl.h>

#include <libplayercore/playercore.h>

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

  private: int device_count;
  private: const char** device_names;
  private: int* sensors_per_device;
  private: int total_sensor_count;
  private: player_pose3d_t* poses;
  private: float range_maximum;
  private: float range_slope;
  private: int sleep_nsec;

  // Bugfix : terminate called without an active exception
  private: int thread_run;
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
    this->device_count = 1;
  else
    this->device_count = tuple_count;

  this->device_names = new const char*[this->device_count];

  for(int i = 0 ; i < this->device_count ; i++) 
  {
    this->device_names[i] =cf->ReadTupleString(section, "devices", i,
                                               "/dev/irsensor0");
  }

  this->total_sensor_count = 0;

  this->sensors_per_device = new int[this->device_count];
  for(int i = 0 ; i < this->device_count ; i++)
  {
    this->sensors_per_device[i] = cf->ReadTupleInt(section, "sensorcount",
                                                   i, 1);
    total_sensor_count += this->sensors_per_device[i];
  }

  this->poses = new player_pose3d_t[this->total_sensor_count];

  for(int i = 0 ; i < this->total_sensor_count ; i++)
  {
    poses[i].px = cf->ReadTupleFloat(section, "sensorposes", i*6, 0);
    poses[i].py = cf->ReadTupleFloat(section, "sensorposes", i*6+1, 0);
    poses[i].pz = cf->ReadTupleFloat(section, "sensorposes", i*6+2, 0);
    poses[i].proll = cf->ReadTupleFloat(section, "sensorposes", i*6+3, 0);
    poses[i].ppitch = cf->ReadTupleFloat(section, "sensorposes", i*6+4, 0);
    poses[i].pyaw = cf->ReadTupleFloat(section, "sensorposes", i*6+5, 0);
  }

  this->range_maximum = cf->ReadFloat(section, "range_maximum", 1.0);
  this->range_slope = cf->ReadFloat(section, "range_slope", 1.0);

  this->sleep_nsec = cf->ReadInt(section, "sleep_nsec", 10000000);
}

// Destructor.  Destroy any created objects.
BeBotIR::~BeBotIR()
{
  delete [] this->device_names;
  delete [] this->sensors_per_device;
  delete [] this->poses;
}

// Set up the device.  Return 0 if things go well, and -1 otherwise.
int BeBotIR::Setup()
{
  // Start the device thread; spawns a new thread and executes
  this->thread_run = 1;
  this->StartThread();

  return 0;
}

// Shutdown the device (called by server thread).
int BeBotIR::Shutdown()
{
  // Stop and join the driver thread
  this->thread_run = 0;
//  StopThread();

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

    pose.poses_count = this->total_sensor_count;
    pose.poses = this->poses;

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

    // Process incoming messages.  BeBotIR::ProcessMessage() is
    // called on each message.
    ProcessMessages();

    // Interact with the device, and push out the resulting data, using
    // Driver::Publish()
//    pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &oldstate);

    unsigned short buffer[this->total_sensor_count];
    unsigned short * p_buffer = buffer;

    // read voltage values in mV
    for( int i = 0; i < this->device_count ; i++)
    {
      int devicefile = open(this->device_names[i], O_RDONLY);
      //printf("%d count: %d p_buffer: %p file: %s \n", devicefile,
      //       this->sensors_per_device[i],
      //       (void*)p_buffer, this->device_names[i]);
      read(devicefile,(void*)p_buffer,
        this->sensors_per_device[i] * sizeof(unsigned short));
      close(devicefile);
      p_buffer += this->sensors_per_device[i];
    }

    float voltages[this->total_sensor_count];
    float ranges[this->total_sensor_count];

    for(int i = 0; i < this->total_sensor_count ; i++)
    {
      //printf("volt:%x %d\n", buffer[i], buffer[i]);
      voltages[i] = buffer[i] * 0.001; // voltage in V
      if(voltages[i]>0)
        ranges[i] = 1.0 / voltages[i] + 4.0;
      if(ranges[i]<0)
        ranges[i] = 0;
      if(ranges[i]>14.0)
        ranges[i] = 14.0;
    }

    player_ir_data_t ir_data;
    ir_data.voltages_count = this->total_sensor_count;
    ir_data.voltages = voltages;
    ir_data.ranges_count = this->total_sensor_count;
    ir_data.ranges = ranges;

    this->Publish(device_addr,
                  PLAYER_MSGTYPE_DATA,
                  PLAYER_IR_DATA_RANGES,
                  (void*) &ir_data,
                  sizeof(ir_data),
                  NULL);
//    pthread_setcancelstate(oldstate, NULL);
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
