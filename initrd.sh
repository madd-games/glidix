#! /bin/sh
# This script is ran in the build directory when "make initrd" is ran.
# The destination file is passed as an argument.

initmod=""

for modname in `ls modconf`
do
	if [ "`cat modconf/$modname`" = "initmod" ]
	then
		initmod="$initmod modules/$modname.gkm"
	fi
done

if [ "$ROOTDEV" = "" ]
then
	ROOTDEV="/dev/sda0"
fi

if [ "$ROOTFS" = "" ]
then
	ROOTFS="gxfs"
fi

command="build-tools/mkinitrd --kernel=kernel/out/kernel.so --init=init/init --output=$1/vmglidix.tar --no-default-initmod $initmod"
echo $command
$command || exit 1
cp kernel/out/kernel.so $1/kernel.so
