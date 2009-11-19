/*
 *  olsrd.cc - Player driver for olsr deamon
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
 * This driver provides a position2d and opaque interface for a olsr deamon.
 */

/** @ingroup drivers */
/** @{ */
/** @defgroup driver_olsr olsr
 * @brief olsr topology

The olsr driver transports topology maps.

@par Compile-time dependencies

@par Provides

- @ref position2d
- @ref opaque

@par Configuration requests

- PLAYER_POSITION2D_REQ_GET_GEOM

@par Configuration file options

- position (float tuple)
  - Default: [0 0 0]
  - Offset position

- size (float tuple)
  - Default: [0.090 0.090]
  - Object size

- ip (string)
  - Default: 127.0.0.1
  - Host running olsrd

- port (integer)
  - Default: 2006
  - Port of olsrd txtinfo plugin

- sleep_nsec (integer)
  - Default 250000000
  - timespec value for nanosleep()

@author Stefan Herbrechtsmeier

*/
/** @} */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <libplayercore/playercore.h>

struct link
{
  uint16_t start;
  uint16_t end;
  float value;
};

// The class for the driver
class Olsrd : public Driver
{
  // Constructor
  public: Olsrd(ConfigFile* cf, int section);

  // Setup/shutdown routines.
  public: virtual int Setup();
  public: virtual int Shutdown();

  // This method will be invoked on each incoming message
  public: virtual int ProcessMessage(QueuePointer &resp_queue,
                               player_msghdr * hdr,
                               void * data);

  // Main function for device thread.
  private: virtual void Main();

  private: int updateTopology();

  private: player_devaddr_t position_addr;
  private: player_devaddr_t opaque_addr;
  private: player_pose2d_t position;
  private: player_position2d_geom_t geometry;
  private: const char *ip;
  private: struct in_addr address; 
  private: int port;
  private: int sleep_nsec;
  private: struct link *topology;
};

// Initialization function.
Driver* Olsrd_Init(ConfigFile* cf, int section)
{
  // Create and return a new instance of this driver
  return((Driver*)(new Olsrd(cf, section)));
}

// Driver registration function
void Olsrd_Register(DriverTable* table)
{
  table->AddDriver("olsrd", Olsrd_Init);
}

// Constructor
Olsrd::Olsrd(ConfigFile* cf, int section)
    : Driver(cf,
             section,
             false,
             PLAYER_MSGQUEUE_DEFAULT_MAXLEN)
{
  memset(&this->position_addr, 0, sizeof(player_devaddr_t));
  memset(&this->opaque_addr, 0, sizeof(player_devaddr_t));

  if (cf->ReadDeviceAddr(&(this->position_addr), section, "provides",
                         PLAYER_POSITION2D_CODE, -1, NULL) == 0)
  {
    if (this->AddInterface(this->position_addr) != 0)
    {
      this->SetError(-1);
      return;
    }
  }

  if (cf->ReadDeviceAddr(&(this->opaque_addr), section, "provides",
                         PLAYER_OPAQUE_CODE, -1, NULL) == 0)
  {
    if (this->AddInterface(this->opaque_addr) != 0)
    {
      this->SetError(-1);
      return;
    }
  }

  this->position.px = cf->ReadTupleFloat(section, "position", 0, 0);
  this->position.py = cf->ReadTupleFloat(section, "position", 1, 0);
  this->position.pa = cf->ReadTupleFloat(section, "position", 2, 0);

  this->geometry.pose.px = 0.0;
  this->geometry.pose.py = 0.0;
  this->geometry.pose.pz = 0.0;
  this->geometry.pose.proll = 0.0;
  this->geometry.pose.ppitch = 0.0;
  this->geometry.pose.pyaw = 0.0;
  this->geometry.size.sl = cf->ReadTupleFloat(section, "size", 0, 0.09);
  this->geometry.size.sw = cf->ReadTupleFloat(section, "size", 1, 0.09);

  this->ip = cf->ReadString(section, "ip", "127.0.0.1");

  this->port = cf->ReadInt(section, "port", 2006);

  this->sleep_nsec = cf->ReadInt(section, "sleep_nsec", 250000000);
}

// Set up the device.  Return 0 if things go well, and -1 otherwise.
int Olsrd::Setup()
{
  int rc = inet_pton(AF_INET, ip, &this->address);
  if (rc <= 0)
  {
    PLAYER_ERROR1("Host %s is not a valid ip address\n", ip);
    return -1;
  }

  StartThread();

  return 0;
}

// Shutdown the device
int Olsrd::Shutdown()
{
  StopThread();

  return 0;
}

int Olsrd::ProcessMessage(QueuePointer & resp_queue,
                          player_msghdr * hdr,
                          void * data)
{
  assert (hdr);

  // Process messages here.  Send a response if necessary, using Publish().
  if (Message::MatchMessage(hdr,
				 PLAYER_MSGTYPE_REQ,
				 PLAYER_POSITION2D_REQ_MOTOR_POWER,
				 this->position_addr))
  {
    this->Publish(this->position_addr, resp_queue,
		  PLAYER_MSGTYPE_RESP_ACK,
		  PLAYER_POSITION2D_REQ_MOTOR_POWER);
    return 0;
  } else if(Message::MatchMessage(hdr,
                                  PLAYER_MSGTYPE_REQ,
                                  PLAYER_POSITION2D_REQ_GET_GEOM,
                                  position_addr))
  {
    this->Publish(this->position_addr, resp_queue,
                  PLAYER_MSGTYPE_RESP_ACK,
                  PLAYER_POSITION2D_REQ_GET_GEOM,
                  (void*)&this->geometry, sizeof(this->geometry), NULL);
    return 0;
  }

  return -1;
}

// Main function for device thread
void Olsrd::Main() 
{
  struct timespec ts;
  int count;

  while(1)
  {
    pthread_testcancel();

    ProcessMessages();

    player_opaque_data_t opaque;

    count = this->updateTopology();
    if (count > 0)
    {
      opaque.data_count = count * sizeof(struct link);
      opaque.data = reinterpret_cast<uint8_t*>(this->topology);
      this->Publish(this->opaque_addr,
                    PLAYER_MSGTYPE_DATA,
                    PLAYER_OPAQUE_DATA_STATE,
                    reinterpret_cast<void*>(&opaque));

/*    PLAYER_ERROR2("--- %i * %i ---\n", count, sizeof(struct link));
    for (int i = 0; i < count; i++)
      PLAYER_ERROR3("%3i <-> %3i = %f\n", this->topology[i].start,
                    this->topology[i].end, this->topology[i].value);
*/

      delete [] this->topology;
      this->topology = NULL;
    }

    this->Publish(this->position_addr,
                  PLAYER_MSGTYPE_DATA,
                  PLAYER_POSITION2D_DATA_STATE,
                  reinterpret_cast<void*>(&this->position));

    ts.tv_sec =0;
    ts.tv_nsec = this->sleep_nsec;
    nanosleep(&ts, NULL);
  }
}

int Olsrd::updateTopology()
{
  struct sockaddr_in sockaddr;
  struct in_addr addr;
  char buffer[1024], ipa[16], ipb[16];
  char *str, *tmp;
  int fd, rc, n, size, count, i;
  float value;
  uint16_t a, b, start, end;

  memset(&sockaddr, 0, sizeof(struct sockaddr_in));

  sockaddr.sin_family = AF_INET;
  sockaddr.sin_addr = this->address;
  sockaddr.sin_port = htons(this->port);

  fd = socket(PF_INET, SOCK_STREAM, 0);
  if (fd < 0)
  {
    PLAYER_ERROR("Cannot create socket\n");
    return -1;
  }

  if (connect(fd, (const struct sockaddr *) &sockaddr,
              sizeof(struct sockaddr_in)) < 0)
  {
    PLAYER_ERROR("Connect failed\n");
    close(fd);
    return -1;
  }

  if (write(fd, "/topo\n", 6) != 6)
  {
    PLAYER_ERROR("Write failed\n");
    close(fd);
    return -1;
  }

  n = read(fd, buffer, sizeof(buffer));
  if (n < 0)
  {
    PLAYER_ERROR("Read failed\n");
    close(fd);
    return -1;
  } else if (n <= 92) {
    close(fd);
    return 0;
  }

  /* remove header */
  str = buffer + 91;
  /* remove tail */
  buffer[n - 1] = '\0';

  tmp = str;
  for(size = 0; *tmp; size++)
  {
     tmp = strchr(tmp, '\n') + 1;
  }

  this->topology = new struct link[size];

  for(count = 0; count < size && *str;)
  {
    rc = sscanf(str, "%s\t%s\t%*f\t%*f\t%f",
                reinterpret_cast<char *>(&ipa),
                reinterpret_cast<char *>(&ipb), &value);
    if (rc != 3)
       break;

    inet_pton(AF_INET, ipa, &addr);
    a = htonl(addr.s_addr) & 0xff;
    inet_pton(AF_INET, ipb, &addr);
    b = htonl(addr.s_addr) & 0xff;

    if (a < b)
    {
      start = a;
      end = b;
    } else {
      start = b;
      end = a;
    }

    for (i = 0;
         i < count && (this->topology[i].start != start ||
                       this->topology[i].end != end);
         i++);

    if (i < count)
    {
      this->topology[i].value = (this->topology[i].value + value) / 2;
    } else {
      this->topology[count].start = start;
      this->topology[count].end = end;
      this->topology[count].value = value;
      count++;
    }

    str = strchr(str, '\n') + 1;
  }

  shutdown(fd, SHUT_RDWR);

  close(fd);

  return count;
}

// Extra stuff for building a shared object.

/* need the extern to avoid C++ name-mangling  */
extern "C" {
  int player_driver_init(DriverTable* table)
  {
    puts("Olsrd driver initializing");
    Olsrd_Register(table);
    puts("Olsrd driver done");
    return 0;
  }
}
