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

command="build-tools/mkinitrd --kernel=kernel/out/kernel.so --init=init/init --rootdev=$ROOTDEV --rootfs=$ROOTFS --output=$1 --no-default-initmod $initmod"
echo $command
$command
