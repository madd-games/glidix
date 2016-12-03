#! /bin/sh
srcdir="`dirname $0`"

echo >Makefile "SRCDIR := $srcdir"
echo >>Makefile "HOST_GCC := $HOST_GCC"
echo >>Makefile "HOST_AS := $HOST_AS"
echo >>Makefile "HOST_AR := $HOST_AR"
echo >>Makefile "HOST_RANLIB := $HOST_RANLIB"
echo >>Makefile "SYSROOT := $GLIDIX_SYSROOT"

cat >>Makefile $srcdir/libc.mk
