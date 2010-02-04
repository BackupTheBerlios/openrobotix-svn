configdir = $(prefix)/share/player/config

CONFIGS =

if WITH_BEBOT
  CONFIGS += bebot.cfg
endif

if WITH_CAMERAV4L2
  CONFIGS += camerav4l2.cfg
  if WITH_BEBOT
    CONFIGS += bebot_cam.cfg
  endif
endif

if WITH_OLSRD
  CONFIGS += olsrd.cfg
endif

config_DATA = $(CONFIGS)

EXTRA_DIST = $(CONFIGS)
