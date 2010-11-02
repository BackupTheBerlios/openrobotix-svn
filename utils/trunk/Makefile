CC=gcc
INSTALL=install
INSTALLDIR=
PREFIX=/usr
BINDIR=/bin
LDFLAGS=
CFLAGS=

all: avoid wiibot

avoid: avoid.c
	$(CC) $(CFLAGS) avoid.c $(LDFLAGS) -o avoid

wiibot: wiibot.c
	$(CC) $(CFLAGS) wiibot.c $(LDFLAGS) -lcwiid -o wiibot

install: avoid wiibot
	$(INSTALL) -d $(INSTALLDIR)$(PREFIX)$(BINDIR)
	$(INSTALL) -m 755 avoid $(INSTALLDIR)$(PREFIX)$(BINDIR)/
	$(INSTALL) -m 755 wiibot $(INSTALLDIR)$(PREFIX)$(BINDIR)/

clean:
	@rm -f *.o avoid wiibot