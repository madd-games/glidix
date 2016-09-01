CFLAGS=-I /glidix/kernel-include
TARGET_CC=x86_64-glidix-gcc
TARGET_MODCC=x86_64-glidix-modcc
out/ne2k.gkm: build/ne2k.o
	x86_64-glidix-modld $@ $^
-include build/ne2k.d
build/ne2k.d: ne2k.c
	set -e; rm -f $@; \
	$(TARGET_CC) -M $(CFLAGS) $< > $@.$$$$; \
	sed 's,ne2k.o[ :]*,build/ne2k.o $@ : ,g' < $@.$$$$ > $@; \
	rm -f $@.$$$$

build/ne2k.o: ne2k.c
	$(TARGET_MODCC) $< $@
