.PHONY: all

all: mkmip mkinitrd sysman

mkmip: $(SRCDIR)/../shutils/mkmip.c
	$(BUILD_GCC) $< -o $@

mkinitrd: $(SRCDIR)/../shutils/mkinitrd.c
	$(BUILD_GCC) $< -o $@

sysman: $(SRCDIR)/../shutils/sysman.c
	$(BUILD_GCC) $< -o $@

