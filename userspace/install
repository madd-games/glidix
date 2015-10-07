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
mount /dev/sda0 /mnt
mkdir /mnt/mnt
mkdir /mnt/bin
mkdir /mnt/etc
mkdir /mnt/etc/init
cp /bin/sh /mnt/bin/sh
cp /bin/mount /mnt/bin/mount
cp /setup.sh /mnt/etc/init/startup.sh
echo "complete!"
