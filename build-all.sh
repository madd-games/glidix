#! /bin/sh
cd userspace/libc
make || exit 1
sudo make install || exit 1
mkdir -p ../../initrd/lib
cp out/lib/libc.so ../../initrd/lib/libc.so
cd ..
cd sh
make || exit 1
mkdir -p ../../isodir/bin
cp out/sh ../../isodir/bin/sh || exit 1
cd ..
cp setup.sh ../isodir/setup.sh || exit 1
cp install.sh ../isodir/install.sh || exit 1
cp startup.conf ../initrd/startup.conf || exit 1
cp startup-fallback.conf ../initrd/startup-fallback.conf || exit 1
mkdir -p ../isodir/init || exit 1
cp init/login.sh ../isodir/init/login.sh || exit 1
cp init/startup.sh ../isodir/init/startup.sh || exit 1
cp fstab ../isodir/fstab || exit 1
cd shutils
make || exit 1
rm -rf ../../mipdir
mkdir -p ../../mipdir/usr/bin
cp -RT out ../../mipdir/usr/bin || exit 1
cp out/mount ../../isodir/bin/mount || exit 1
cd ..
x86_64-glidix-gcc init.c -o ../initrd/init || exit 1
mkdir -p ../isodir/bin
x86_64-glidix-gcc gxpart.c -o ../isodir/bin/gxpart -I ../include || exit 1
x86_64-glidix-gcc mkgxfs.c -o ../isodir/bin/mkgxfs -I ../include || exit 1
x86_64-glidix-gcc ldmods.c -o ../isodir/ldmods || exit 1
cd ..
sudo make install-dev || exit 1
mkdir -p initrd/initmod

# bootloader
cd userspace/gxld
export PATH=$PATH:/usr/i686-elf/bin
make || exit 1
x86_64-glidix-gcc gxld-install.c -o ../../isodir/bin/gxld-install || exit 1
cp mbr.bin ../../isodir/boot/mbr.bin
cp stage2.bin ../../isodir/boot/stage2.bin
cd ../..

# shutils
chmod 6755 mipdir/usr/bin/passwd
chmod 6755 mipdir/usr/bin/sudo
chmod 6755 mipdir/usr/bin/ping
mkmip mipdir isodir/shutils.mip
cp mipdir/usr/bin/mip-install isodir/bin/mip-install

# modtools
rm -rf mipdir
mkdir -p mipdir/usr/bin
mkdir -p mipdir/usr/lib
nasm -felf64 utils/module_start.asm -o mipdir/usr/lib/module_start.o || exit 1
nasm -felf64 utils/module_end.asm -o mipdir/usr/lib/module_end.o || exit 1
cp utils/module.ld mipdir/usr/lib/module.ld || exit 1
cp -r include mipdir/usr/kernel-include
cp utils/native-modtools/modcc mipdir/usr/bin/modcc
cp utils/native-modtools/modld mipdir/usr/bin/modld
cp utils/modmake mipdir/usr/bin/modmake
chmod +x mipdir/usr/bin/modcc
chmod +x mipdir/usr/bin/modld
chmod +x mipdir/usr/bin/modmake
mkmip mipdir isodir/modtools.mip

# libddi and GUI
rm -rf mipdir
mkdir -p mipdir/usr/lib
mkdir -p mipdir/usr/bin
mkdir -p mipdir/usr/share
mkdir -p mipdir/usr/libexec

cd userspace/libddi
sudo cp libddi.h /glidix/usr/include/libddi.h || exit 1
x86_64-glidix-gcc -I/glidix/usr/include/freetype2 -fPIC -shared libddi.c -o ../../mipdir/usr/lib/libddi.so -lfreetype -lpng -lz -lm || exit 1
sudo cp ../../mipdir/usr/lib/libddi.so /glidix/usr/lib/libddi.so || exit 1
cd ../..

cd userspace/gui
#x86_64-glidix-gcc lgitest.c -o ../isodir/bin/lgitest -I ../include -lddi || exit 1
sudo cp gui.h /glidix/usr/include/libgwm.h || exit 1
x86_64-glidix-gcc gui.c -o ../../mipdir/usr/bin/gui -I ../../include -lddi -Wall -Werror || exit 1
cd libgwm
x86_64-glidix-gcc -fPIC -shared libgwm.c button.c msgbox.c textfield.c -o ../../../mipdir/usr/lib/libgwm.so -lddi || exit 1
cd ..
sudo cp ../../mipdir/usr/lib/libgwm.so /glidix/usr/lib/libgwm.so || exit 1
x86_64-glidix-gcc gui-init.c -o ../../mipdir/usr/libexec/gui-init -lddi -lgwm || exit 1
x86_64-glidix-gcc terminal.c font.c -o ../../mipdir/usr/bin/terminal -lddi -lgwm || exit 1
x86_64-glidix-gcc gui-login.c -o ../../mipdir/usr/bin/gui-login -lddi -lgwm -lcrypt || exit 1
cd ../..
cp -r userspace/images mipdir/usr/share/images || exit 1
cp -r userspace/fonts mipdir/usr/share/fonts || exit 1
mkmip mipdir isodir/gui.mip
#exit

# libc
rm -rf mipdir
mkdir -p mipdir/lib
cd userspace/libc
sh install.sh ../../mipdir
cd ../..
mkmip mipdir isodir/libc.mip

# libc-dev
rm -rf mipdir
cd userspace/libc
sh install-dev.sh ../../mipdir
cd ../..
mkmip mipdir isodir/libc-dev.mip

# shell
rm -rf mipdir
mkdir -p mipdir/bin
cp userspace/sh/out/sh mipdir/bin/sh
mkmip mipdir isodir/sh.mip

# mod_ps2kbd
cd modules/mod_ps2kbd
modmake --sysroot=/glidix --host=x86_64-glidix --modname=ps2kbd || exit 1
cp out/ps2kbd.gkm ../../initrd/initmod/ps2kbd.gkm || exit 1
cd ../..

# mod_specdevs
cd modules/mod_specdevs
modmake --sysroot=/glidix --host=x86_64-glidix --modname=specdevs || exit 1
cp out/specdevs.gkm ../../initrd/initmod/specdevs.gkm || exit 1
cd ../..

# mod_sdide
cd modules/mod_sdide
modmake --sysroot=/glidix --host=x86_64-glidix --modname=sdide || exit 1
cp out/sdide.gkm ../../initrd/sdide.gkm || exit 1
cd ../..

# mod_sdahci
# Do not place it as an initmode yet - experimental and stuff
cd modules/mod_sdahci
modmake --sysroot=/glidix --host=x86_64-glidix --modname=sdahci || exit 1
cp out/sdahci.gkm ../../initrd/initmod/sdahci.gkm || exit 1
cd ../..

# mod_isofs
cd modules/mod_isofs
modmake --sysroot=/glidix --host=x86_64-glidix --modname=isofs || exit 1
cp out/isofs.gkm ../../initrd/initmod/isofs.gkm || exit 1
cd ../..

# mod_gxfs
cd modules/mod_gxfs
modmake --sysroot=/glidix --host=x86_64-glidix --modname=gxfs || exit 1
cp out/gxfs.gkm ../../initrd/initmod/gxfs.gkm || exit 1
cd ../..

# mod_bga
cd modules/mod_bga
modmake --sysroot=/glidix --host=x86_64-glidix --modname=bga || exit 1
cp out/bga.gkm ../../initrd/initmod/bga.gkm || exit 1
cd ../..

# mod_ne2k
cd modules/mod_ne2k
modmake --sysroot=/glidix --host=x86_64-glidix --modname=ne2k || exit 1
cp out/ne2k.gkm ../../initrd/initmod/ne2k.gkm || exit 1
cd ../..

# mod_vionet
cd modules/mod_vionet
modmake --sysroot=/glidix --host=x86_64-glidix --modname=vionet || exit 1
cp out/vionet.gkm ../../initrd/initmod/vionet.gkm || exit 1
cd ../..

x86_64-glidix-gcc -I/glidix/usr/include/freetype2 userspace/test.c -o isodir/bin/test -lz -lfreetype -lddi -lgwm || exit 1

rm -f out/vmglidix.tar
make || exit 1

cd out
#autodrop glidix.iso
cd ..
#autodrop isodir/libc.mip -R GlidixPkg
#autodrop isodir/libc-dev.mip -R GlidixPkg
#autodrop isodir/shutils.mip -R GlidixPkg
#autodrop isodir/sh.mip -R GlidixPkg
#autodrop isodir/libc-dev.mip -R GlidixPkg
