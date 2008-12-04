/*
 * camerav4l2.cc - Driver for Video4Linux2 based cameras
 *
 * Copyright (C) 2008
 *   Heinz Nixdorf Institute - University of Paderborn
 *   Department of System and Circuit Technology
 *   Daniel Klimeck <Daniel.Klimeck@gmx.de>
 *   Stefan Herbrechtsmeier <hbmeier@hni.uni-paderborn.de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

/** @ingroup drivers */
/** @{ */
/** @defgroup driver_camerav4l2 camerav4l2
 * @brief Video4Linux2 camera capture

The camerav4l2 driver captures images from V4L2-compatible cameras.

@par Compile-time dependencies

- libv4l2

@par Provides

- @ref interface_camera

@par Requires

- none

@par Configuration requests

- none

@par Configuration file options

- port (integer)
  - Default: "/dev/video0"
  - Device to read video data from.

- size (integer tuple)
  - Default: [320, 240]
  - Desired image size.   This may not be honoured if the driver does
    not support the requested size).

- fps (integer)
  - Default: 0 (do not set)
  - Requested frame rate (frames/second).

- gain (integer)
  - Default: -1 (do not set)
  - Sets the camera gain setting.

- h_flip (integer)
  - Default: 0 (do not set)
  - Horizontal Flip.

- sleep_nsec (integer)
  - Default: 10000000 (=10ms which gives max 100 fps)
  - timespec value for nanosleep()

@par Example 

@verbatim
driver
(
  name "camerav4l2"
  plugin "libcamerav4l2"
  provides ["camera:0"]
  port "/dev/video0"
  size [320 240]
)
@endverbatim

@author Daniel Klimeck, Stefan Herbrechtsmeier
*/
/** @} */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <time.h>

#include <sys/ioctl.h>

#include <linux/videodev2.h>

#include <libv4l2.h>

#include <libplayercore/playercore.h>
#include <libplayercore/error.h>

// Time for timestamps
extern PlayerTime *GlobalTime;

// The class for the CameraV4L2 driver
class CameraV4L2 : public Driver
{
  // Constructor
  public: CameraV4L2(ConfigFile* cf, int section);

  // Setup/shutdown routines.
  public: virtual int Setup();
  public: virtual int Shutdown();

  // This method will be invoked on each incoming message
  public: virtual int ProcessMessage(QueuePointer &resp_queue, 
                               player_msghdr * hdr,
                               void * data);

  // Main function for device thread.
  private: virtual void Main();

  // Initialization of the device
  private: int InitDevice();

   // Save a frame to memory
  private: int GrabFrame();

  // Update the device data (the data going back to the client)
  private: void RefreshData();

  // Set framerate
  private: int SetFramerate(int fps);

  // Video device
  private: const char *device;

  // Image dimensions
  private: int width, height;

  // Frame grabber interface
  private: int fd;

  // Data to send to server
  private: player_camera_data_t * data;

  private: int fps;

  private: int gain;

  private: int h_flip;

  private: int sleep_nsec;

  // Capture timestamp
  private: double timestamp;

  // Bugfix : terminate called without an active exception
  private: int thread_run;
};

// Initialization function
Driver* CameraV4L2_Init(ConfigFile* cf, int section)
{
  return reinterpret_cast<Driver *>(new CameraV4L2(cf, section));
}

// Driver registration function
void CameraV4L2_Register(DriverTable* table)
{
  table->AddDriver("camerav4l2", CameraV4L2_Init);
}

// Constructor
CameraV4L2::CameraV4L2(ConfigFile* cf, int section)
    : Driver(cf,
             section,
             true,
             PLAYER_MSGQUEUE_DEFAULT_MAXLEN,
             PLAYER_CAMERA_CODE), fd(-1), data(NULL)
{
  // Camera defaults to /dev/video0
  this->device = cf->ReadString(section, "port", "/dev/video0");

  // Size
  this->width = cf->ReadTupleInt(section, "size", 0, 320);
  this->height = cf->ReadTupleInt(section, "size", 1, 240);

  this->fps = cf->ReadInt(section, "fps", 0);

  this->gain = cf->ReadInt(section, "gain", -1);

  this->h_flip = cf->ReadInt(section, "h_flip", 0);

  this->sleep_nsec = cf->ReadInt(section, "sleep_nsec", 10000000);
}

// Set up the device.  Return 0 if things go well, and -1 otherwise.
int CameraV4L2::Setup()
{
  this->fd = v4l2_open(this->device, O_RDWR, 0);
  if (this->fd == -1)
  {
    PLAYER_ERROR1("Couldn't open capture device %s", this->device);
    return -1;
  }

  if (this->InitDevice() == -1)
    return -1;

  // Start the device thread; spawns a new thread and executes
  this->thread_run = 1;
  this->StartThread();

  return 0;
}

// Shutdown the device (called by server thread).
int CameraV4L2::Shutdown()
{
  // Stop and join the driver thread
  this->thread_run = 0;
//  StopThread();

  // Free resources
  if (v4l2_close(this->fd) == -1)
  {
    PLAYER_ERROR("Couldn't close file handle");
    return -1;
  }

  return 0;
}

// Process an incoming message
int CameraV4L2::ProcessMessage(QueuePointer &resp_queue,
                               player_msghdr * hdr,
                               void * data)
{
  assert(hdr);
  assert(data);

  return -1;
}

// Main function for device thread
void CameraV4L2::Main()
{
//  int oldstate;
  struct timespec tspec;
  int rc;

  // The main loop; interact with the device here
  while(this->thread_run)
  {
    // Test if we are supposed to cancel this thread.
    pthread_testcancel();

    // Go to sleep for a while.
    tspec.tv_sec = 0;
    tspec.tv_nsec = this->sleep_nsec;
    nanosleep(&tspec, NULL);

    ProcessMessages();

//    pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &oldstate);

    // Grab the next frame (blocking)
    rc = this->GrabFrame();
    if (rc)
    {
      PLAYER_ERROR("No frame!");
      continue;
    }

    // Interact with the device, and push out the resulting data.
    this->RefreshData();

//    pthread_setcancelstate(oldstate, NULL);
  }
}

// Initialization of the device
int CameraV4L2::InitDevice()
{
  struct v4l2_capability cap;
  struct v4l2_cropcap cropcap;
  struct v4l2_crop crop;
  struct v4l2_format fmt;

  if (v4l2_ioctl(this->fd, VIDIOC_QUERYCAP, &cap) == -1)
  {
    if (EINVAL == errno)
    {
      PLAYER_ERROR1("[%s] is no V4L2 device", this->device);
      return -1;
    }
    else
    {
      PLAYER_ERROR("Unsupported VIDIOC_QUERYCAP");
      return -1;
    }
  }

  if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE))
  {
    PLAYER_ERROR1("[%s] is no video capture device", this->device);
    return -1;
  }

  if (!(cap.capabilities & V4L2_CAP_STREAMING))
  {
    PLAYER_ERROR1("[%s] does not support streaming i/o", this->device);
    return -1;
  }

  if (!(cap.capabilities & V4L2_CAP_READWRITE))
  {
    PLAYER_ERROR1("[%s] does not support read i/o", this->device);
    return -1;
  }

  // Select video input, video standard and tune here
  memset(&cropcap, 0, sizeof(cropcap));
  cropcap.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

  if (v4l2_ioctl(this->fd, VIDIOC_CROPCAP, &cropcap) == 0)
  {
    crop.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    crop.c = cropcap.defrect; /* reset to default */

    if (v4l2_ioctl(this->fd, VIDIOC_S_CROP, &crop) == -1)
    {
      switch (errno) 
      {
        case EINVAL:
          /* Cropping not supported. */
          break;
        default:
          /* Errors ignored. */
          break;
      }
    }
  }
  else
  {
    /* Errors ignored. */
  }

  memset(&fmt, 0, sizeof(fmt));
  unsigned int color = V4L2_PIX_FMT_RGB24;

  fmt.type                = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  fmt.fmt.pix.width       = this->width;
  fmt.fmt.pix.height      = this->height;
  fmt.fmt.pix.pixelformat = color;
  fmt.fmt.pix.field       = V4L2_FIELD_ANY;

  if (v4l2_ioctl(this->fd, VIDIOC_S_FMT, &fmt) == -1)
  {
    PLAYER_ERROR("Unsupported VIDIOC_S_FMT");
    return -1;
  }

  if (fmt.fmt.pix.pixelformat != color)
  {
    PLAYER_ERROR4("Pixelformat (%c%c%c%c) not supported\n",
      (char) fmt.fmt.pix.pixelformat,
      (char) (fmt.fmt.pix.pixelformat >> 8),
      (char) (fmt.fmt.pix.pixelformat >> 16),
      (char) (fmt.fmt.pix.pixelformat >> 24));
    return -1;
  }

  // Note VIDIOC_S_FMT may change width and height.
  this->width = fmt.fmt.pix.width;
  this->height = fmt.fmt.pix.height;

  // Set Gain
  if (this->gain != -1) {
    if (v4l2_set_control(this->fd,V4L2_CID_GAIN, this->gain) == -1)
      PLAYER_ERROR("Gain control not supported\n");
  }

  // Set Horizontal Flip
  if(this->h_flip != 0){
    if(v4l2_set_control(this->fd, V4L2_CID_HFLIP, this->h_flip) == -1)
      PLAYER_ERROR(" Horizontal Flip is not supported\n");
  }

  // Set framerate
  if (this->fps != 0) {
    if (this->SetFramerate(this->fps) == -1)
      PLAYER_ERROR("Set framerate not supported\n");
  }

  return 0;
}

// Store an image frame into the 'frame' buffer
int CameraV4L2::GrabFrame()
{
  struct timeval time;

  // Get the time
  GlobalTime->GetTime(&time);
  this->timestamp = time.tv_sec + time.tv_usec * 1.e-6;

  assert(!(this->data));
  this->data = reinterpret_cast<player_camera_data_t *>
    (malloc(sizeof(player_camera_data_t)));
  assert(this->data);

  this->data->bpp = 24;
  this->data->format = PLAYER_CAMERA_FORMAT_RGB888;
  this->data->fdiv = 1;
  this->data->compression = PLAYER_CAMERA_COMPRESS_RAW;
  this->data->image_count = this->width * this->height * 3;
  this->data->image = reinterpret_cast<uint8_t *>
    (malloc(this->data->image_count));
  assert(this->data->image);
  this->data->width = this->width;
  this->data->height = this->height;

  if (v4l2_read(this->fd, this->data->image, this->data->image_count) <= 0)
  {
    free(this->data->image);
    this->data->image = NULL;
    free(this->data);
    this->data = NULL;
    return -1;
  }

  return 0;
}

// Update the device data (the data going back to the client).
void CameraV4L2::RefreshData()
{
  assert(this->data);
  if (!(this->data->image_count))
  {
    free(this->data->image);
    free(this->data);
    PLAYER_ERROR("No image data to publish");
    return;
  }

  assert(this->data->image);
  Publish(this->device_addr, 
          PLAYER_MSGTYPE_DATA, PLAYER_CAMERA_DATA_STATE,
          reinterpret_cast<void *>(this->data),
          0, &this->timestamp, false);

  // We assume that Publish() freed this data!
  this->data = NULL;

  return;
}


// Set Framerate
int CameraV4L2::SetFramerate(int fps)
{
  struct v4l2_streamparm streamparm;

  if (fps > 0) {
    memset(&streamparm, 0, sizeof(streamparm));
    streamparm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (v4l2_ioctl(this->fd, VIDIOC_G_PARM, &streamparm) == -1) {
      if (errno != EINVAL) {
        PLAYER_ERROR("Unsupported VIDIOC_G_PARM");
        return -1;
      } else 
        PLAYER_ERROR("VIDIOC_G_PARM is not supported");
    } else if (!(streamparm.parm.capture.capability & V4L2_CAP_TIMEPERFRAME))
        PLAYER_ERROR("V4L2_CAP_TIMEPERFRAME is not supported");
    else {
      memset(&streamparm, 0, sizeof(streamparm));
      streamparm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
      streamparm.parm.capture.timeperframe.numerator = 1;
      streamparm.parm.capture.timeperframe.denominator = fps;
      if (-1 == v4l2_ioctl(this->fd, VIDIOC_S_PARM, &streamparm)){
        PLAYER_ERROR("Unsupported VIDIOC_S_PARM");
        return -1;
      }
    }
  }

  return 0;
}

// Extra stuff for building a shared object.

/* need the extern to avoid C++ name-mangling  */
extern "C" {
  int player_driver_init(DriverTable* table)
  {
    puts("CameraV4L2 driver initializing");
    CameraV4L2_Register(table);
    puts("CameraV2L2 driver done");
    return 0;
  }
}
