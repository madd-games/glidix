#! /bin/sh
srcdir="`dirname $0`"

echo >Makefile "SRCDIR := $srcdir"
echo >>Makefile "HOST_GCC := $HOST_GCC"
echo >>Makefile "HOST_AS := $HOST_AS"
echo >>Makefile "SYSROOT := $GLIDIX_SYSROOT"
echo >>Makefile "BUILD_GCC := $BUILD_GCC"

cat >>Makefile $srcdir/build-tools.mk
