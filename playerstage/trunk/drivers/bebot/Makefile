# CameraV4L plugin makefile
LIBTOOL = libtool
INSTALL= install -c
CFLAGS = $(shell pkg-config --cflags playercore)
LDFLAGS = 
VERSION = 2:1:1
DESTDIR =
prefix = /usr
libdir = $(prefix)/lib
cfgdir = $(prefix)/share/player/config

all: libbebotir.la libbebotmotor.la

%.lo: %.cc
	$(LIBTOOL) --mode=compile $(CXX) -c $(CFLAGS) $<

libbebotir.la: bebot_ir.lo
	$(LIBTOOL) --mode=link $(CXX) $(LDFLAGS) -rpath $(libdir) -module \
	-export-dynamic -version-number $(VERSION) $^ -o $@

libbebotmotor.la: bebot_motor.lo
	$(LIBTOOL) --mode=link $(CXX) $(LDFLAGS) -rpath $(libdir) -module \
	-export-dynamic -version-number $(VERSION) $^ -o $@

install: install-lib install-cfg

install-lib: libcamerav4l2.la
	$(LIBTOOL) --mode=install ${INSTALL} libbebotir.la $(DESTDIR)$(libdir)/
	$(LIBTOOL) --mode=install ${INSTALL} libbebotmotor.la $(DESTDIR)$(libdir)/

install-cfg:
	${INSTALL} -m 644 bebot.cfg $(DESTDIR)$(cfgdir)/

clean:
	rm -rf *.o *.lo *.la .libs *~
