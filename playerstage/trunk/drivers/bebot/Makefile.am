lib_LTLIBRARIES =

if WITH_BEBOT
  lib_LTLIBRARIES += libplayerdriver_bebobir.la libplayerdriver_bebotmotor.la
endif

libplayerdriver_bebobir_la_SOURCES = bebot_ir.cc
libplayerdriver_bebobir_la_CPPFLAGS = ${PLAYERCORE_CFLAGS}
libplayerdriver_bebobir_la_LIBADD = $(PLAYERCORE_LIBS) ${VERSION_INFO}

libplayerdriver_bebotmotor_la_SOURCES = bebot_motor.cc
libplayerdriver_bebotmotor_la_CPPFLAGS = ${PLAYERCORE_CFLAGS}
libplayerdriver_bebotmotor_la_LIBADD = $(PLAYERCORE_LIBS) ${VERSION_INFO}
