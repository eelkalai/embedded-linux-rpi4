# Out-of-tree build of the BNO085 driver.
#
#   make                                   # build bno085.ko
#   make install INSTALL_MOD_PATH=<rootfs> # install under <rootfs>/lib/modules/<version>/updates/
#   make clean
#
# ARCH and CROSS_COMPILE come from the environment, as for the kernel build.
# KDIR must be the kernel tree that runs on the board.

ifneq ($(KERNELRELEASE),)
# Called by the kernel's build system (Kbuild)
obj-m := bno085.o
else
KDIR ?= $(HOME)/embedded-linux/embedded-linux-bbb-labs/kernel/linux

all:
	$(MAKE) -C $(KDIR) M=$(CURDIR) modules

install:
	$(MAKE) -C $(KDIR) M=$(CURDIR) modules_install

clean:
	$(MAKE) -C $(KDIR) M=$(CURDIR) clean

.PHONY: all install clean
endif
