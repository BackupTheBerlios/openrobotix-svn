lib_LTLIBRARIES =

if WITH_BEBOT
  lib_LTLIBRARIES += libplayerdriver_bebotir.la libplayerdriver_bebotmotor.la
endif

libdir = @PLAYERCORE_PLUGIN_DIR@

libplayerdriver_bebotir_la_SOURCES = bebot_ir.cc
libplayerdriver_bebotir_la_CPPFLAGS = ${PLAYERCORE_CFLAGS}
libplayerdriver_bebotir_la_LDFLAGS = -avoid-version
libplayerdriver_bebotir_la_LIBADD = $(PLAYERCORE_LIBS)

libplayerdriver_bebotmotor_la_SOURCES = bebot_motor.cc
libplayerdriver_bebotmotor_la_CPPFLAGS = ${PLAYERCORE_CFLAGS}
libplayerdriver_bebotmotor_la_LDFLAGS = -avoid-version
libplayerdriver_bebotmotor_la_LIBADD = $(PLAYERCORE_LIBS)
