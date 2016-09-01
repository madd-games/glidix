CFLAGS=-I /glidix/kernel-include
TARGET_CC=x86_64-glidix-gcc
TARGET_MODCC=x86_64-glidix-modcc
out/isofs.gkm: build/isofile.o build/isodir.o build/main.o
	x86_64-glidix-modld $@ $^
-include build/isofile.d build/isodir.d build/main.d
build/isofile.d: isofile.c
	set -e; rm -f $@; \
	$(TARGET_CC) -M $(CFLAGS) $< > $@.$$$$; \
	sed 's,isofile.o[ :]*,build/isofile.o $@ : ,g' < $@.$$$$ > $@; \
	rm -f $@.$$$$

build/isofile.o: isofile.c
	$(TARGET_MODCC) $< $@

build/isodir.d: isodir.c
	set -e; rm -f $@; \
	$(TARGET_CC) -M $(CFLAGS) $< > $@.$$$$; \
	sed 's,isodir.o[ :]*,build/isodir.o $@ : ,g' < $@.$$$$ > $@; \
	rm -f $@.$$$$

build/isodir.o: isodir.c
	$(TARGET_MODCC) $< $@

build/main.d: main.c
	set -e; rm -f $@; \
	$(TARGET_CC) -M $(CFLAGS) $< > $@.$$$$; \
	sed 's,main.o[ :]*,build/main.o $@ : ,g' < $@.$$$$ > $@; \
	rm -f $@.$$$$

build/main.o: main.c
	$(TARGET_MODCC) $< $@
