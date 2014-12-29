CFLAGS=-I /glidix/kernel-include
TARGET_CC=x86_64-glidix-gcc
TARGET_MODCC=x86_64-glidix-modcc
out/ps2kbd.gkm: build/ps2kbd.o
	x86_64-glidix-modld $@ $<
-include build/ps2kbd.d
build/ps2kbd.d: ps2kbd.c
	set -e; rm -f $@; \
	$(TARGET_CC) -M $(CFLAGS) $< > $@.$$$$; \
	sed 's,ps2kbd.o[ :]*,build/ps2kbd.o $@ : ,g' < $@.$$$$ > $@; \
	rm -f $@.$$$$

build/ps2kbd.o: ps2kbd.c
	$(TARGET_MODCC) $< $@
