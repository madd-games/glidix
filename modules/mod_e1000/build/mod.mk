CFLAGS=-I /glidix/kernel-include
TARGET_CC=x86_64-glidix-gcc
TARGET_MODCC=x86_64-glidix-modcc
out/e1000.gkm: build/e1000.o
	x86_64-glidix-modld $@ $^
-include build/e1000.d
build/e1000.d: e1000.c
	set -e; rm -f $@; \
	$(TARGET_CC) -M $(CFLAGS) $< > $@.$$$$; \
	sed 's,e1000.o[ :]*,build/e1000.o $@ : ,g' < $@.$$$$ > $@; \
	rm -f $@.$$$$

build/e1000.o: e1000.c
	$(TARGET_MODCC) $< $@
