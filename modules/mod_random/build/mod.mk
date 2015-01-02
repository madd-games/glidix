CFLAGS=-I /glidix/kernel-include
TARGET_CC=x86_64-glidix-gcc
TARGET_MODCC=x86_64-glidix-modcc
out/random.gkm: build/random.o
	x86_64-glidix-modld $@ $^
-include build/random.d
build/random.d: random.c
	set -e; rm -f $@; \
	$(TARGET_CC) -M $(CFLAGS) $< > $@.$$$$; \
	sed 's,random.o[ :]*,build/random.o $@ : ,g' < $@.$$$$ > $@; \
	rm -f $@.$$$$

build/random.o: random.c
	$(TARGET_MODCC) $< $@
