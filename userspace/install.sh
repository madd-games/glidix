# first-format.sh
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
