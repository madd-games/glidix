# install.sh
echo "*** ATTENTION ***"
echo "The partition editor will now be started. Please create a partition in slot 0"
echo "for the Glidix installation, or shut down your PC. When you exit from the"
echo "partition editor, the partition will be formatted and Glidix will be"
echo "installed from the CD."
echo "*** END ATTENTION ***"
echo

gxpart /dev/sda
mkgxfs /dev/sda0
gxld-ins /boot/mbr.bin /boot/stage2.bin /dev/sda
mount /dev/sda0 /mnt
mkdir /mnt/boot
cp /boot/vmglidix /mnt/boot/vmglidix
cp /boot/vmglidix.tar /mnt/boot/vmglidix.tar
mkdir /mnt/mnt
mkdir /mnt/bin
mkdir /mnt/etc
mkdir /mnt/etc/init
mkdir /mnt/etc/services
mkdir /mnt/usr
mkdir /mnt/usr/libexec
cp /bin/sh /mnt/bin/sh
cp /bin/mount /mnt/bin/mount
cp /setup.sh /mnt/etc/init/startup.sh
cp /ldmods /mnt/usr/libexec/ldmods
echo "complete!"
