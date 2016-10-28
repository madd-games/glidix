CFLAGS=-I /glidix/kernel-include
TARGET_CC=x86_64-glidix-gcc
TARGET_MODCC=x86_64-glidix-modcc
out/gxfs.gkm: build/gxfs.o build/gxblock.o build/gxdir.o build/gxfile.o build/gxinode.o
	x86_64-glidix-modld $@ $^
-include build/gxfs.d build/gxblock.d build/gxdir.d build/gxfile.d build/gxinode.d
build/gxfs.d: gxfs.c
	set -e; rm -f $@; \
	$(TARGET_CC) -M $(CFLAGS) $< > $@.$$$$; \
	sed 's,gxfs.o[ :]*,build/gxfs.o $@ : ,g' < $@.$$$$ > $@; \
	rm -f $@.$$$$

build/gxfs.o: gxfs.c
	$(TARGET_MODCC) $< $@

build/gxblock.d: gxblock.c
	set -e; rm -f $@; \
	$(TARGET_CC) -M $(CFLAGS) $< > $@.$$$$; \
	sed 's,gxblock.o[ :]*,build/gxblock.o $@ : ,g' < $@.$$$$ > $@; \
	rm -f $@.$$$$

build/gxblock.o: gxblock.c
	$(TARGET_MODCC) $< $@

build/gxdir.d: gxdir.c
	set -e; rm -f $@; \
	$(TARGET_CC) -M $(CFLAGS) $< > $@.$$$$; \
	sed 's,gxdir.o[ :]*,build/gxdir.o $@ : ,g' < $@.$$$$ > $@; \
	rm -f $@.$$$$

build/gxdir.o: gxdir.c
	$(TARGET_MODCC) $< $@

build/gxfile.d: gxfile.c
	set -e; rm -f $@; \
	$(TARGET_CC) -M $(CFLAGS) $< > $@.$$$$; \
	sed 's,gxfile.o[ :]*,build/gxfile.o $@ : ,g' < $@.$$$$ > $@; \
	rm -f $@.$$$$

build/gxfile.o: gxfile.c
	$(TARGET_MODCC) $< $@

build/gxinode.d: gxinode.c
	set -e; rm -f $@; \
	$(TARGET_CC) -M $(CFLAGS) $< > $@.$$$$; \
	sed 's,gxinode.o[ :]*,build/gxinode.o $@ : ,g' < $@.$$$$ > $@; \
	rm -f $@.$$$$

build/gxinode.o: gxinode.c
	$(TARGET_MODCC) $< $@
