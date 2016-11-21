#! /bin/sh
srcdir="`dirname $0`"

echo >Makefile "SRCDIR := $srcdir"
echo >>Makefile "HOST_GCC := $HOST_GCC"
echo >>Makefile "HOST_AS := $HOST_AS"
echo >>Makefile "SYSROOT := $GLIDIX_SYSROOT"
echo >>Makefile "GLIDIX_VERSION := $GLIDIX_VERSION"

cat >>Makefile $srcdir/loader.mk
