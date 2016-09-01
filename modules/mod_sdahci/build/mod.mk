CFLAGS=-I /glidix/kernel-include
TARGET_CC=x86_64-glidix-gcc
TARGET_MODCC=x86_64-glidix-modcc
out/sdahci.gkm: build/sdahci.o
	x86_64-glidix-modld $@ $^
-include build/sdahci.d
build/sdahci.d: sdahci.c
	set -e; rm -f $@; \
	$(TARGET_CC) -M $(CFLAGS) $< > $@.$$$$; \
	sed 's,sdahci.o[ :]*,build/sdahci.o $@ : ,g' < $@.$$$$ > $@; \
	rm -f $@.$$$$

build/sdahci.o: sdahci.c
	$(TARGET_MODCC) $< $@
