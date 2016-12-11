.PHONY: build install

build: gcc-build/Makefile
	cd gcc-build && ac_cv_c_bigendian=no make
	
gcc-build/Makefile: gcc-$(GCC_VERSION)/configure
	mkdir -p gcc-build
	cd gcc-build && ../gcc-$(GCC_VERSION)/configure --host=x86_64-glidix --target=x86_64-glidix --prefix=/usr --enable-languages=c,c++ --enable-multilib --enable-multiarch --disable-nls --with-pkgversion='Glidix GCC'
	
gcc-$(GCC_VERSION)/configure:
	rm -f gcc-$(GCC_VERSION).tar.gz
	wget http://nl.mirror.babylon.network/gcc/releases/gcc-$(GCC_VERSION)/gcc-$(GCC_VERSION).tar.gz
	rm -rf gcc-$(GCC_VERSION)
	tar -xf gcc-$(GCC_VERSION).tar.gz
	cd gcc-$(GCC_VERSION) && ./contrib/download_prerequisites
	cp $(SRCDIR)/gcc-$(GCC_VERSION).patch gcc-$(GCC_VERSION)/gcc.patch
	cd gcc-$(GCC_VERSION) && patch -p1 < gcc.patch
	rm gcc-$(GCC_VERSION)/gcc.patch

install:
	cd gcc-build && make install

