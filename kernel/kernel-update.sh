#! /bin/sh
# This script shall be extracted in /tmp/kernel-update along with kernel.so
# and init, and executed in that directory. It creates an initrd containing
# the new kernel, in the normal path (../../boot/vmglidix.tar). The environment
# variable INITRD_OPTIONS may contain additional options to be passed to
# mkinitrd.
mkinitrd --kernel=kernel.so --output=../../boot/vmglidix.tar --init=init $INITRD_OPTIONS || echo "$0: mkinitrd failed!" && exit 1

# Delete the temp files
rm kernel.so kernel-update.sh >/dev/null 2>&1

# Print a nice notice for the user.
echo "+----------------------------NOTICE----------------------------+"
echo "|                                                              |"
echo "| The kernel has been installed successfully. You must now     |"
echo "| reboot for the changes to take effect. This can be done with |"
echo "| the following command:                                       |"
echo "|                                                              |"
echo "|     sudo reboot                                              |"
echo "|                                                              |"
echo "+--------------------------------------------------------------+"
