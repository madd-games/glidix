#! /bin/sh
srcdir="`dirname $0`"

echo >Makefile "SRCDIR := $srcdir"
echo >>Makefile "HOST_GCC := $HOST_GCC"
echo >>Makefile "HOST_AS := $HOST_AS"
echo >>Makefile "SYSROOT := $GLIDIX_SYSROOT"
echo >>Makefile "GCC_VERSION := $GLIDIX_GCC_VERSION"

if [ ! -f $srcdir/gcc-$GLIDIX_GCC_VERSION.patch ]
then
	echo >&2 "gcc $GLIDIX_GCC_VERSION is not supported!"
	exit 1
fi

cat >>Makefile $srcdir/gcc.mk
