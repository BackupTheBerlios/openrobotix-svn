lib_LTLIBRARIES =

if WITH_BEBOT
  lib_LTLIBRARIES += libplayerdriver_bebotir.la libplayerdriver_bebotmotor.la
endif

libplayerdriver_bebotir_la_SOURCES = bebot_ir.cc
libplayerdriver_bebotir_la_CPPFLAGS = ${PLAYERCORE_CFLAGS}
libplayerdriver_bebotir_la_LIBADD = $(PLAYERCORE_LIBS) ${VERSION_INFO}

libplayerdriver_bebotmotor_la_SOURCES = bebot_motor.cc
libplayerdriver_bebotmotor_la_CPPFLAGS = ${PLAYERCORE_CFLAGS}
libplayerdriver_bebotmotor_la_LIBADD = $(PLAYERCORE_LIBS) ${VERSION_INFO}
