ifndef KERNEL_SRC
	KERNEL_SRC := /lib/modules/$(shell uname -r)/build
endif

KBUILD_PARAMS := -C $(KERNEL_SRC) M=$(CURDIR)

all:
	@$(MAKE) ${KBUILD_PARAMS} modules

clean:
	@$(MAKE) ${KBUILD_PARAMS} clean
	@rm -f Module.symvers *~

modules_install:
	$(MAKE) $(KBUILD_PARAMS) modules_install
