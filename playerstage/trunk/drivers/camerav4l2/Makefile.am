lib_LTLIBRARIES =

if WITH_CAMERAV4L2
  lib_LTLIBRARIES += libplayerdriver_camerav4l2.la
endif

libdir = @PLAYERCORE_PLUGIN_DIR@

libplayerdriver_camerav4l2_la_SOURCES = camerav4l2.cc
libplayerdriver_camerav4l2_la_CPPFLAGS = $(PLAYERCORE_CFLAGS) ${LIBV4L2_CFLAGS}
libplayerdriver_camerav4l2_la_LIBADD = $(PLAYERCORE_LIBS) $(LIBV4L2_LIBS) \
  $(LIBV4LCONVERT_LIBS) ${VERSION_INFO}
