.PHONY: build install

build: binutils-build/Makefile
	cd binutils-build && make
	
binutils-build/Makefile: binutils-$(BINUTILS_VERSION)/configure
	mkdir -p binutils-build
	cd binutils-build && ../binutils-$(BINUTILS_VERSION)/configure --host=x86_64-glidix --target=x86_64-glidix --prefix=/usr --disable-werror --disable-nls
	
binutils-$(BINUTILS_VERSION)/configure:
	rm -f binutils-$(BINUTILS_VERSION).tar.gz
	wget https://ftp.gnu.org/gnu/binutils/binutils-$(BINUTILS_VERSION).tar.gz
	rm -rf binutils-$(BINUTILS_VERSION)
	tar -xf binutils-$(BINUTILS_VERSION).tar.gz
	cp $(SRCDIR)/binutils-$(BINUTILS_VERSION).patch binutils-$(BINUTILS_VERSION)/binutils.patch
	cd binutils-$(BINUTILS_VERSION) && patch -p1 < binutils.patch
	rm binutils-$(BINUTILS_VERSION)/binutils.patch

install:
	cd binutils-build && make install

