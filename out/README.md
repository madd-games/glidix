Standard Distribution CD Image
==============================

The CD image `glidix.iso` is the "standard distribution" image. Upon booting up on a system where Glidix is not installed, or a broken Glidix installation is present, it will automatically format the hard drive, install a "basic system", and request a reboot. When booting up the second time, the full OS will be installed, and when you reboot once again, the system is ready to use.

On the default distribution OS, the Glidix shell and the shell utilities (`shutils`) are all installed. Currently, no programming environment is available (this is coming soon). The CD is mounted under `/media/cdrom` (as specified in `/etc/fstab`). To force a full reinstallation of the OS, just delete the shell using `rm /bin/sh` and `reboot`. This will install everything again, starting with the initial format.

When the OS first boots, you are asked to create a partition. To do this easily, you can wipe the partition table by typing `clear`, then create a new partition by typing `create` and keep pressing ENTER, entering default options, then type `setboot` to make it bootable, and finally `done` followed by a `yes`.

When the full system installation is complete, you can still use GRUB on the CD to boot it, or you can simply remove the CD and let the Glidix Loader boot the kernel.
