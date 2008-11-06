lib_LTLIBRARIES =

if WITH_BEBOT
  lib_LTLIBRARIES += libbebotir.la libbebotmotor.la
endif

libdir = @PLAYER_PLUGIN_DIR@

libbebotir_la_SOURCES = bebot_ir.cc
libbebotir_la_CPPFLAGS = ${PLAYERCORE_CFLAGS}
libbebotir_la_LDFLAGS = -avoid-version
libbebotir_la_LIBADD = $(PLAYERCORE_LIBS)

libbebotmotor_la_SOURCES = bebot_motor.cc
libbebotmotor_la_CPPFLAGS = ${PLAYERCORE_CFLAGS}
libbebotmotor_la_LDFLAGS = -avoid-version
libbebotmotor_la_LIBADD = $(PLAYERCORE_LIBS)
