lib_LTLIBRARIES =

if WITH_OLSRD
  lib_LTLIBRARIES += libolsrd.la
endif

libdir = @PLAYER_PLUGIN_DIR@

AM_CPPFLAGS = -Wall -g $(PLAYERCORE_CFLAGS)

libolsrd_la_SOURCES = olsrd.cc
libolsrd_la_LDFLAGS = -avoid-version
libolsrd_la_LIBADD = $(PLAYERCORE_LIBS)
