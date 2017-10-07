#! /bin/sh
srcdir="`dirname $0`"

echo >Makefile "SRCDIR := $srcdir"
echo >>Makefile "HOST_GCC := $HOST_GCC"
echo >>Makefile "HOST_AS := $HOST_AS"
echo >>Makefile "SYSROOT := $GLIDIX_SYSROOT"
echo >>Makefile "CFLAGS := -Wall -Werror -fPIC -ggdb -I\$(SRCDIR)/../libddi -O3"

# Get the list of drivers
driver_list_tmp="`ls -l $srcdir | grep '^d' | awk 'NF>1{print $NF}'`"
driver_list="`echo $driver_list_tmp`"

# Generate makefile list
so_list=""
for driver in $driver_list
do
	so_list="$so_list ${driver}.so"
done

echo >>Makefile "DRIVERS := $so_list"
echo >>Makefile ".PHONY: all install"
echo >>Makefile "all: \$(DRIVERS)"
echo >>Makefile "install:"
echo >>Makefile "	@mkdir -p \$(DESTDIR)/usr/lib/ddidrv"

for driver in $driver_list
do
	echo >>Makefile "	cp ${driver}.so \$(DESTDIR)/usr/lib/ddidrv/${driver}.so"
done

for driver in $driver_list
do
	cat $srcdir/driver.mk.in | sed "s/DRIVER_NAME/$driver/g" >> Makefile
done

cat $srcdir/driver.mk.end >> Makefile
