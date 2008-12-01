lib_LTLIBRARIES =

if WITH_CAMERAV4L2
  lib_LTLIBRARIES += libcamerav4l2.la
endif

libdir = @PLAYER_PLUGIN_DIR@

AM_CPPFLAGS = -Wall -g $(PLAYERCORE_CFLAGS) $(LIBV4L2_CFLAGS)

libcamerav4l2_la_SOURCES = camerav4l2.cc
libcamerav4l2_la_LDFLAGS = -avoid-version
libcamerav4l2_la_LIBADD = $(PLAYERCORE_LIBS) $(LIBV4L2_LIBS) \
  $(LIBV4LCONVERT_LIBS)
