CFLAGS=-I /glidix/kernel-include
TARGET_CC=x86_64-glidix-gcc
TARGET_MODCC=x86_64-glidix-modcc
out/vionet.gkm: build/vionet.o
	x86_64-glidix-modld $@ $^
-include build/vionet.d
build/vionet.d: vionet.c
	set -e; rm -f $@; \
	$(TARGET_CC) -M $(CFLAGS) $< > $@.$$$$; \
	sed 's,vionet.o[ :]*,build/vionet.o $@ : ,g' < $@.$$$$ > $@; \
	rm -f $@.$$$$

build/vionet.o: vionet.c
	$(TARGET_MODCC) $< $@
