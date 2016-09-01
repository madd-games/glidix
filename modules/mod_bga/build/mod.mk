CFLAGS=-I /glidix/kernel-include
TARGET_CC=x86_64-glidix-gcc
TARGET_MODCC=x86_64-glidix-modcc
out/bga.gkm: build/bga.o
	x86_64-glidix-modld $@ $^
-include build/bga.d
build/bga.d: bga.c
	set -e; rm -f $@; \
	$(TARGET_CC) -M $(CFLAGS) $< > $@.$$$$; \
	sed 's,bga.o[ :]*,build/bga.o $@ : ,g' < $@.$$$$ > $@; \
	rm -f $@.$$$$

build/bga.o: bga.c
	$(TARGET_MODCC) $< $@
