# If KERNELRELEASE is defined, we've been invoked from the
# kernel build system and can use its language.
MODCFLAGS := -Wall -DMODULE -D__KERNEL__ -DLINUX -std=c99
ifneq ($(KERNELRELEASE),)
obj-m := hello.o
export EXTRA_CFLAGS := -std=gnu99 -DKERNEL_DEBUG
# Otherwise we were called directly from the command
# line; invoke the kernel build system.
else
	KERNELDIR ?= /lib/modules/$(shell uname -r)/build
	PWD  := $(shell pwd)
default:
	make -C $(KERNELDIR) M=$(PWD) modules
clean:
	-rm -f *.o *.ko .*.cmd .*.flags *.mod.c
endif