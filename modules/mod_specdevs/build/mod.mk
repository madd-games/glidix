CFLAGS=-I /glidix/kernel-include
TARGET_CC=x86_64-glidix-gcc
TARGET_MODCC=x86_64-glidix-modcc
out/specdevs.gkm: build/specdevs.o
	x86_64-glidix-modld $@ $^
-include build/specdevs.d
build/specdevs.d: specdevs.c
	set -e; rm -f $@; \
	$(TARGET_CC) -M $(CFLAGS) $< > $@.$$$$; \
	sed 's,specdevs.o[ :]*,build/specdevs.o $@ : ,g' < $@.$$$$ > $@; \
	rm -f $@.$$$$

build/specdevs.o: specdevs.c
	$(TARGET_MODCC) $< $@
