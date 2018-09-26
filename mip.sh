#! /bin/sh

rm -rf mipdir
mkdir mipdir || exit 1
mipdir="`realpath mipdir`"
mkdir -p $1

export DESTDIR="$mipdir"

srcdir="`dirname $0`"

# libc
cd libc
make install || exit 1
echo "// dummy" > ../mipdir/usr/include/limits.h || exit 1		# to stop GCC complaining
cd ..
build-tools/mkmip mipdir $1/libc.mip -i libc $GLIDIX_VERSION -d initrd $GLIDIX_VERSION || exit 1

# shutils
rm -rf mipdir
mkdir mipdir || exit 1
cd shutils
make install || exit 1
cd ..
build-tools/mkmip mipdir $1/shutils.mip --setup=/usr/libexec/shutils-setup.sh -i shutils $GLIDIX_VERSION -d libc $GLIDIX_VERSION -d libz $GLIDIX_VERSION -d libgpm $GLIDIX_VERSION || exit 1

# modules
rm -rf mipdir
mkdir mipdir || exit 1
cd modules
make install || exit 1
cd ..
build-tools/mkmip mipdir $1/modules.mip -i modules $GLIDIX_VERSION -d initrd $GLIDIX_VERSION

# sh
rm -rf mipdir
mkdir mipdir || exit 1
cd sh
make install || exit 1
cd ..
build-tools/mkmip mipdir $1/sh.mip -i sh $GLIDIX_VERSION -d libc $GLIDIX_VERSION

# binutils
if [ "$MIP_BINUTILS" = "yes" ]
then
	rm -rf mipdir
	mkdir mipdir || exit 1
	cd binutils
	make install || exit 1
	cd ..
	build-tools/mkmip mipdir $1/binutils.mip -i binutils 2.27 -d libc 0.1
fi

# gcc
if [ "$MIP_GCC" = "yes" ]
then
	rm -rf mipdir
	mkdir mipdir || exit 1
	cd gcc
	make install || exit 1
	cd ..
	build-tools/mkmip mipdir $1/gcc.mip -i gcc 6.2.0 -d libc 0.1 -d binutils 2.27
fi

# libz
rm -rf mipdir || exit 1
cd libz
make install || exit 1
cd ..
build-tools/mkmip mipdir $1/libz.mip -i libz $GLIDIX_VERSION -d libc $GLIDIX_VERSION

# dynld
rm -rf mipdir || exit 1
cd dynld
make install || exit 1
cd ..
build-tools/mkmip mipdir $1/dynld.mip -i dynld $GLIDIX_VERSION -d initrd $GLIDIX_VERSION

# libpng
rm -rf mipdir || exit 1
cd libpng
make install || exit 1
cd ..
build-tools/mkmip mipdir $1/libpng.mip -i libpng 1.6.21 -d libc 0.1

# freetype
rm -rf mipdir || exit 1
cd freetype
make install || exit 1
cd ..
build-tools/mkmip mipdir $1/freetype.mip -i freetype 2.6.3 -d libc 0.1

# libddi
rm -rf mipdir || exit 1
cd libddi
make install || exit 1
cd ..
build-tools/mkmip mipdir $1/libddi.mip -i libddi $GLIDIX_VERSION -d libc $GLIDIX_VERSION

# libgl
rm -rf mipdir || exit 1
cd libgl
make install || exit 1
cd ..
build-tools/mkmip mipdir $1/libgl.mip -i libgl 3.0 -d libc $GLIDIX_VERSION -d libddi $GLIDIX_VERSION

# libgwm
rm -rf mipdir || exit 1
cd libgwm || exit 1
make install || exit 1
cd ..
build-tools/mkmip mipdir $1/libgwm.mip -i libgwm $GLIDIX_VERSION -d libc $GLIDIX_VERSION -d libddi $GLIDIX_VERSION

# gwmserver
rm -rf mipdir || exit 1
cd gwmserver || exit 1
make install || exit 1
cd ..
build-tools/mkmip mipdir $1/gwmserver.mip -i gwmserver $GLIDIX_VERSION -d libc $GLIDIX_VERSION -d libgwm $GLIDIX_VERSION -d libddi $GLIDIX_VERSION

# guiapps
rm -rf mipdir || exit 1
cd guiapps || exit 1
make install || exit 1
cd ..
build-tools/mkmip mipdir $1/guiapps.mip -i guiapps $GLIDIX_VERSION -d libc $GLIDIX_VERSION -d libgwm $GLIDIX_VERSION -d libddi $GLIDIX_VERSION

# fstools
rm -rf mipdir || exit 1
cd fstools
make install || exit 1
cd ..
build-tools/mkmip mipdir $1/fstools.mip -i fstools $GLIDIX_VERSION -d libc $GLIDIX_VERSION

# netman
rm -rf mipdir || exit 1
cd netman
make install || exit 1
cd ..
build-tools/mkmip mipdir $1/netman.mip -i netman $GLIDIX_VERSION -d libc $GLIDIX_VERSION

# klogd
rm -rf mipdir || exit 1
cd klogd
make install || exit 1
cd ..
build-tools/mkmip mipdir $1/klogd.mip -i klogd $GLIDIX_VERSION -d libc $GLIDIX_VERSION

# sysinfo
rm -rf mipdir || exit 1
cd sysinfo
make install || exit 1
cd ..
build-tools/mkmip mipdir $1/sysinfo.mip -i sysinfo $GLIDIX_VERSION -d libc $GLIDIX_VERSION  -d libgwm $GLIDIX_VERSION -d libddi $GLIDIX_VERSION

# filemgr
rm -rf mipdir || exit 1
cd filemgr
make install || exit 1
cd ..
build-tools/mkmip mipdir $1/filemgr.mip -i filemgr $GLIDIX_VERSION -d libc $GLIDIX_VERSION  -d libgwm $GLIDIX_VERSION -d libddi $GLIDIX_VERSION

# minipad
rm -rf mipdir || exit 1
cd minipad
make install || exit 1
cd ..
build-tools/mkmip mipdir $1/minipad.mip -i minipad $GLIDIX_VERSION -d libc $GLIDIX_VERSION  -d libgwm $GLIDIX_VERSION -d libddi $GLIDIX_VERSION

# gxdbg
rm -rf mipdir || exit 1
cd gxdbg
make install || exit 1
cd ..
build-tools/mkmip mipdir $1/gxdbg.mip -i gxdbg $GLIDIX_VERSION -d libc $GLIDIX_VERSION

# ddidrv
rm -rf mipdir || exit 1
cd ddi-drivers
make install || exit 1
cd ..
build-tools/mkmip mipdir $1/ddidrv.mip -i ddidrv $GLIDIX_VERSION -d libc $GLIDIX_VERSION -d libddi $GLIDIX_VERSION

# gwm-themes
rm -rf mipdir || exit 1
mkdir -p mipdir/usr/share/themes || exit 1
cp -RT $srcdir/themes mipdir/usr/share/themes || exit 1
build-tools/mkmip mipdir $1/gwm-themes.mip -i gwm-themes $GLIDIX_VERSION -d libgwm $GLIDIX_VERSION || exit 1

# libgpm
rm -rf mipdir || exit 1
cd libgpm
make install || exit 1
cd ..
build-tools/mkmip mipdir $1/libgpm.mip -i libgpm $GLIDIX_VERSION -d libc $GLIDIX_VERSION

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
build-tools/mkmip mipdir $1/initrd.mip --setup=/run/mkinitrd/build.sh -i initrd $GLIDIX_VERSION || exit 1
