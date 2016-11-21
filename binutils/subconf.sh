#! /bin/sh
srcdir="`dirname $0`"

echo >Makefile "SRCDIR := $srcdir"
echo >>Makefile "HOST_GCC := $HOST_GCC"
echo >>Makefile "HOST_AS := $HOST_AS"
echo >>Makefile "SYSROOT := $GLIDIX_SYSROOT"
echo >>Makefile "BINUTILS_VERSION := $GLIDIX_BINUTILS_VERSION"

if [ ! -f $srcdir/binutils-$GLIDIX_BINUTILS_VERSION.patch ]
then
	echo >&2 "binutils $GLIDIX_BINUTILS_VERSION is not supported!"
	exit 1
fi

cat >>Makefile $srcdir/binutils.mk
