# setup.sh
# Run this on a newly-formatted Glidix root filesystem to set everything up,
# ready to work!
# Make sure all the needed stuff is under /mnt.
# Also, this must be run as root.

mount /dev/sdb /mnt -t isofs
/mnt/bin/mip-inst /mnt/libc.mip /mnt/sh.mip /mnt/shutils.mip

# Create all the directories for default mounted filesystems.
# If they already exist then an error occurs, just ignore it.
mkdir /sys
mkdir /sys/mod
mkdir /dev
mkdir /initrd
mkdir /proc
mkdir /mnt
mkdir /media
mkdir /media/cdrom
mkdir /etc
mkdir /etc/init
mkdir /run
mkdir /tmp

cp /mnt/fstab /etc/fstab

# We must make additional links to the "halt" program, which will reboot and power off
# the system. The "halt" executable can do all those things, but requires the program
# name to be different to specify the action.
ln /usr/bin/halt /usr/bin/reboot
ln /usr/bin/halt /usr/bin/poweroff

# Now create the admin account!
echo "Setting up the user database!"
pwdsetup

# Copy the scripts.
cp /mnt/init/login.sh /etc/init/login.sh
cp /mnt/init/startup.sh /etc/init/startup.sh
chmod 644 /etc/init/login.sh
chmod 644 /etc/init/startup.sh
echo "Rebooting..."
reboot
