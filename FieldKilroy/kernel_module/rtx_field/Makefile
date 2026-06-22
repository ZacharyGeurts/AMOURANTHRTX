# Out-of-tree RTX Field bridge — builds against running kernel headers by default.
# ABI reference: kilroy-7.1.1 (downloaded to SG/linux-kernel/).
# Override: make KDIR=/path/to/prepared/kilroy-7.1.1

obj-m += rtx_field.o

KVER ?= $(shell uname -r)
KDIR ?= /lib/modules/$(KVER)/build

all:
	$(MAKE) -C $(KDIR) M=$(CURDIR) modules

clean:
	$(MAKE) -C $(KDIR) M=$(CURDIR) clean

install:
	$(MAKE) -C $(KDIR) M=$(CURDIR) modules_install

.PHONY: all clean install