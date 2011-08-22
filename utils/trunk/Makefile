DIRS = bebot fpga

BUILDDIRS = $(DIRS:%=build-%)
INSTALLDIRS = $(DIRS:%=install-%)
CLEANDIRS = $(DIRS:%=clean-%)

all: $(BUILDDIRS)
$(DIRS): $(BUILDDIRS)
$(BUILDDIRS):
	$(MAKE) -e -C $(@:build-%=%) $(MAKEFLAGS)

install: $(INSTALLDIRS)
$(INSTALLDIRS):
	$(MAKE) -e -C $(@:install-%=%)  $(MAKEFLAGS) install

clean: $(CLEANDIRS)
$(CLEANDIRS): 
	$(MAKE) -e -C $(@:clean-%=%)  $(MAKEFLAGS) clean

.PHONY: subdirs $(DIRS)
.PHONY: subdirs $(BUILDDIRS)
.PHONY: subdirs $(INSTALLDIRS)
.PHONY: subdirs $(CLEANDIRS)
.PHONY: all install clean test