include ../Makefile.inc

all: fpga

fpga: fpga.c
	$(CC) $(CFLAGS) fpga.c $(LDFLAGS) -o fpga

install: fpga
	$(INSTALL) -d $(INSTALLDIR)$(BINDIR)
	$(INSTALL) -m 755 fpga $(INSTALLDIR)$(BINDIR)/

	$(INSTALL) -d $(INSTALLDIR)$(LIBDIR)/firmware
	$(INSTALL) -m 644 fpga.bit $(INSTALLDIR)$(LIBDIR)/firmware/

clean:
	@rm -f *.o fpga

