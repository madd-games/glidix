#! /bin/sh

rm -rf mipdir
mkdir mipdir || exit 1
mipdir="`realpath mipdir`"
mkdir -p $1

export DESTDIR="$mipdir"

# libc
cd libc
make install || exit 1
cd ..
build-tools/mkmip mipdir $1/libc.mip || exit 1

# shutils
rm -rf mipdir
mkdir mipdir || exit 1
cd shutils
make install || exit 1
cd ..
build-tools/mkmip mipdir $1/shutils.mip --setup=/usr/libexec/shutils-setup.sh || exit 1

# modules
rm -rf mipdir
mkdir mipdir || exit 1
cd modules
make install || exit 1
cd ..
build-tools/mkmip mipdir $1/modules.mip

# sh
rm -rf mipdir
mkdir mipdir || exit 1
cd sh
make install || exit 1
cd ..
build-tools/mkmip mipdir $1/sh.mip

# binutils
if [ "$MIP_BINUTILS" = "yes" ]
then
	rm -rf mipdir
	mkdir mipdir || exit 1
	cd binutils
	make install || exit 1
	cd ..
	build-tools/mkmip mipdir $1/binutils.mip
fi

# gcc
if [ "$MIP_GCC" = "yes" ]
then
	rm -rf mipdir
	mkdir mipdir || exit 1
	cd gcc
	make install || exit 1
	cd ..
	build-tools/mkmip mipdir $1/gcc.mip
fi

# libz
rm -rf mipdir || exit 1
cd libz
make install || exit 1
cd ..
build-tools/mkmip mipdir $1/libz.mip

# dynld
rm -rf mipdir || exit 1
cd dynld
make install || exit 1
cd ..
build-tools/mkmip mipdir $1/dynld.mip

# libpng
rm -rf mipdir || exit 1
cd libpng
make install || exit 1
cd ..
build-tools/mkmip mipdir $1/libpng.mip

# freetype
rm -rf mipdir || exit 1
cd freetype
make install || exit 1
cd ..
build-tools/mkmip mipdir $1/freetype.mip

# libddi
rm -rf mipdir || exit 1
cd libddi
make install || exit 1
cd ..
build-tools/mkmip mipdir $1/libddi.mip

# libgwm
rm -rf mipdir || exit 1
cd libgwm || exit 1
make install || exit 1
cd ..
build-tools/mkmip mipdir $1/libgwm.mip

# gui
rm -rf mipdir || exit 1
cd gui || exit 1
make install || exit 1
cd ..
build-tools/mkmip mipdir $1/gui.mip

# fstools
rm -rf mipdir || exit 1
cd fstools
make install || exit 1
cd ..
build-tools/mkmip mipdir $1/fstools.mip

# netman
rm -rf mipdir || exit 1
cd netman
make install || exit 1
cd ..
build-tools/mkmip mipdir $1/netman.mip

# sysinfo
rm -rf mipdir || exit 1
cd sysinfo
make install || exit 1
cd ..
build-tools/mkmip mipdir $1/sysinfo.mip
