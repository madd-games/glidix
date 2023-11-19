.PHONY: all

all: mkmip mkinitrd sysman mkgpt gxboot-install

mkmip: $(SRCDIR)/../shutils/mkmip.c
	$(BUILD_GCC) $< -o $@

mkinitrd: $(SRCDIR)/../shutils/mkinitrd.c
	$(BUILD_GCC) $< -o $@

sysman: $(SRCDIR)/../shutils/sysman.c
	$(BUILD_GCC) $< -o $@

mkgpt: $(SRCDIR)/../shutils/mkgpt.c
	$(BUILD_GCC) $< -o $@

gxboot-install: $(SRCDIR)/../gxboot/gxboot-install.c
	$(BUILD_GCC) $< -o $@