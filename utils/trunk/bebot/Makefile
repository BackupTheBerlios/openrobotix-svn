include ../Makefile.inc

all: avoid wiibot

avoid: avoid.c
	$(CC) $(CFLAGS) avoid.c $(LDFLAGS) -o avoid

wiibot: wiibot.c
	$(CC) $(CFLAGS) wiibot.c $(LDFLAGS) -lcwiid -o wiibot

install: avoid wiibot
	$(INSTALL) -d $(INSTALLDIR)$(BINDIR)
	$(INSTALL) -m 755 avoid $(INSTALLDIR)$(BINDIR)/
	$(INSTALL) -m 755 wiibot $(INSTALLDIR)$(BINDIR)/

clean:
	@rm -f *.o avoid wiibot