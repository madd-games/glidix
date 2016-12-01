#! /bin/sh
srcdir="`dirname $0`"

echo >Makefile "SRCDIR := $srcdir"
echo >>Makefile ".PHONY: all install"
echo >>Makefile "all: configure"
echo >>Makefile "	cd build && make"
echo >>Makefile "install:"
echo >>Makefile "	cd build && make install"
echo >>Makefile "configure:"
echo >>Makefile "	@mkdir -p build"
echo >>Makefile "	cd build && ../\$(SRCDIR)/configure --prefix=/usr --host=x86_64-glidix --disable-static"

