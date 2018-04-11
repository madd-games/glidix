#! /bin/sh

rm -rf mipdir
mkdir mipdir || exit 1
mipdir="`realpath mipdir`"
mkdir -p $1

export DESTDIR="$mipdir"

# libc
cd libc
make install || exit 1
echo "// dummy" > ../mipdir/usr/include/limits.h || exit 1		# to stop GCC complaining
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

# gwmserver
rm -rf mipdir || exit 1
cd gwmserver || exit 1
make install || exit 1
cd ..
build-tools/mkmip mipdir $1/gwmserver.mip

# guiapps
rm -rf mipdir || exit 1
cd guiapps || exit 1
make install || exit 1
cd ..
build-tools/mkmip mipdir $1/guiapps.mip

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

# klogd
rm -rf mipdir || exit 1
cd klogd
make install || exit 1
cd ..
build-tools/mkmip mipdir $1/klogd.mip

# sysinfo
rm -rf mipdir || exit 1
cd sysinfo
make install || exit 1
cd ..
build-tools/mkmip mipdir $1/sysinfo.mip

# minipad
rm -rf mipdir || exit 1
cd minipad
make install || exit 1
cd ..
build-tools/mkmip mipdir $1/minipad.mip

# gxdbg
rm -rf mipdir || exit 1
cd gxdbg
make install || exit 1
cd ..
build-tools/mkmip mipdir $1/gxdbg.mip

# ddidrv
rm -rf mipdir || exit 1
cd ddi-drivers
make install || exit 1
cd ..
build-tools/mkmip mipdir $1/ddidrv.mip

# gpm
rm -rf mipdir || exit 1
cd gpm
make install || exit 1
cd ..
build-tools/mkmip mipdir $1/gpm.mip

# initrd
rm -rf mipdir || exit 1
mkdir -p mipdir/run/mkinitrd
cp kernel/out/kernel.so mipdir/run/mkinitrd/kernel.so
cp init/init mipdir/run/mkinitrd/init
initmod=""
for modname in `ls modconf`
do
	if [ "`cat modconf/$modname`" = "initmod" ]
	then
		initmod="$initmod $modname.gkm"
		cp modules/$modname.gkm mipdir/run/mkinitrd/$modname.gkm
	fi
done
echo "#! /bin/sh" > mipdir/run/mkinitrd/build.sh
echo "echo \"Generating the initrd...\"" >> mipdir/run/mkinitrd/build.sh
echo "mkinitrd --kernel=kernel.so --init=init --output=../../boot/vmglidix.tar --no-default-initmod $initmod" >> mipdir/run/mkinitrd/build.sh
echo "echo \"Cleaning up...\"" >> mipdir/run/mkinitrd/build.sh
echo "rm kernel.so init build.sh $initmod" >> mipdir/run/mkinitrd/build.sh
echo "echo \"Done; initmods are: $initmod\"" >> mipdir/run/mkinitrd/build.sh
echo "echo \"*** RESTART THE SYSTEM TO APPLY CHANGES ***\"" >> mipdir/run/mkinitrd/build.sh
chmod +x mipdir/run/mkinitrd/build.sh
build-tools/mkmip mipdir $1/initrd.mip --setup=/run/mkinitrd/build.sh || exit 1
