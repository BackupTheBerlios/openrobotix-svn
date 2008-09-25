# CameraV4L plugin makefile
LIBTOOL = libtool
INSTALL= install -c
CFLAGS = `pkg-config --cflags playercore` `pkg-config --cflags libv4l2`
LDFLAGS = `pkg-config --libs libv4l2` `pkg-config --libs libv4lconvert`
VERSION = 2:1:1
DESTDIR =
prefix = /usr
libdir = $(prefix)/lib
cfgdir = $(prefix)/share/player/config

all: libcamerav4l2.la

%.lo: %.cc
	$(LIBTOOL) --mode=compile $(CXX) -c $(CFLAGS) $<

libcamerav4l2.la: camerav4l2.lo
	$(LIBTOOL) --mode=link $(CXX) $(LDFLAGS) -rpath $(libdir) -module \
	-export-dynamic -version-number $(VERSION) $^ -o $@

install: install-lib install-cfg

install-lib: libcamerav4l2.la
	$(LIBTOOL) --mode=install ${INSTALL} libcamerav4l2.la $(DESTDIR)$(libdir)/libcamerav4l2.la

install-cfg:
	${INSTALL} -m 644 camerav4l2.cfg $(DESTDIR)$(cfgdir)/

clean:
	rm -rf *.o *.lo *.la .libs *~
