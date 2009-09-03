ifndef KERNEL_SRC
	KERNEL_SRC := /lib/modules/$(shell uname -r)/build
endif

ifndef INSTALL_HDR_PATH
	KERNEL_SRC := /usr
endif

KBUILD_PARAMS := -C $(KERNEL_SRC) M=$(CURDIR)
UNIFDEV_PARAMS := -U__KERNEL__

INSTALL = install
RM = rm
UNIFDEF = unifdef

all: modules

help:
	@$(MAKE) ${KBUILD_PARAMS} help

modules:
	@$(MAKE) ${KBUILD_PARAMS} modules

modules_install:
	@$(MAKE) $(KBUILD_PARAMS) modules_install

headers_install:
	$(INSTALL) -d $(INSTALL_HDR_PATH)/include/linux
	- $(UNIFDEF) $(UNIFDEV_PARAMS) include/linux/senseact.h > include/linux/senseact.h.tmp
	$(INSTALL) include/linux/senseact.h.tmp $(INSTALL_HDR_PATH)/include/linux/senseact.h
	$(RM) include/linux/senseact.h.tmp

modules_clean:
	@$(MAKE) ${KBUILD_PARAMS} clean
	@$(RM) -f Module.symvers *~

clean: modules_clean