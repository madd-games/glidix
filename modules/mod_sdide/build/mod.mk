CFLAGS=-I /glidix/kernel-include
TARGET_CC=x86_64-glidix-gcc
TARGET_MODCC=x86_64-glidix-modcc
out/sdide.gkm: build/sdide.o
	x86_64-glidix-modld $@ $<
-include build/sdide.d
build/sdide.d: sdide.c
	set -e; rm -f $@; \
	$(TARGET_CC) -M $(CFLAGS) $< > $@.$$$$; \
	sed 's,sdide.o[ :]*,build/sdide.o $@ : ,g' < $@.$$$$ > $@; \
	rm -f $@.$$$$

build/sdide.o: sdide.c
	$(TARGET_MODCC) $< $@
