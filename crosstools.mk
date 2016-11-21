.PHONY: all binutils gcc install patches

all: binutils gcc

configure:
	mkdir -p $(SYSROOT)/usr/include
	cp -RT $(SRCDIR)/libc/include $(SYSROOT)/usr/include
	rm -rf build-binutils
	mkdir -p build-binutils
	cd build-binutils && ../glidix-binutils/configure --target=x86_64-glidix --prefix=$(SYSROOT)/usr --with-sysroot=$(SYSROOT) --disable-werror --disable-nls
	rm -rf build-gcc
	mkdir -p build-gcc
	cd build-gcc && ../glidix-gcc/configure --target=x86_64-glidix --prefix=$(SYSROOT)/usr --with-sysroot=$(SYSROOT) --enable-languages=c,c++ --enable-multilib --enable-multiarch
	touch configure

binutils: configure
	cd build-binutils && make

gcc: configure
	cd build-gcc && make all-gcc all-target-libgcc

patches:
	diff -urN binutils-$(BINUTILS_VERSION) glidix-binutils > binutils-$(BINUTILS_VERSION).patch
	diff -urN gcc-$(GCC_VERSION) glidix-gcc > gcc-$(GCC_VERSION).patch
	
# NOTE: This target MUST be last, because setup-crosstools-workspace may append commands to the end of this
install:
	cd build-binutils && make install
	cd build-gcc && make install-gcc install-target-libgcc
