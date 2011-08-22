include ../Makefile.inc

PLAYER_LDFLAGS=`pkg-config --libs playerc++`
PLAYER_CFLAGS=`pkg-config --cflags playerc++`

all: follow

follow: follow.cc
	echo $(CFLAGS) $(PLAYER_CFLAGS)
	echo $(LDFLAGS) $(PLAYER_LDLAGS)
	$(CC) $(CFLAGS) $(PLAYER_CFLAGS) follow.cc $(LDFLAGS) $(PLAYER_LDFLAGS) -o follow

install: follow
	$(INSTALL) -d $(INSTALLDIR)$(BINDIR)
	$(INSTALL) -m 755 follow $(INSTALLDIR)$(BINDIR)/

clean:
	@rm -f *.o follow