lib_LTLIBRARIES =

if WITH_BEBOT
  lib_LTLIBRARIES += libbebotir.la libbebotbase.la
endif

libdir = @PLAYER_PLUGIN_DIR@

AM_CPPFLAGS = -Wall -g $(PLAYERCORE_CFLAGS) $(SENSEACT_CFLAGS)

libbebotir_la_SOURCES = bebot_ir.cc
libbebotir_la_LDFLAGS = -avoid-version
libbebotir_la_LIBADD = $(PLAYERCORE_LIBS)

libbebotbase_la_SOURCES = bebot_base.cc
libbebotbase_la_LDFLAGS = -avoid-version
libbebotbase_la_LIBADD = $(PLAYERCORE_LIBS)
