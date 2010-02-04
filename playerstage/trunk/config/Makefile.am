configdir = $(prefix)/share/player/config

CONFIGS =

if WITH_BEBOT
  CONFIGS += bebot.cfg
endif

if WITH_CAMERAV4L2
  CONFIGS += camerav4l2.cfg
endif

if WITH_CAMERAV4L2_BEBOT
  CONFIGS += bebot_cam.cfg
endif

if WITH_OLSRD
  CONFIGS += olsrd.cfg
endif

config_DATA = $(CONFIGS)

EXTRA_DIST = $(CONFIGS)
