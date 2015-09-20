Standard Distribution CD Image
==============================

The CD image `glidix.iso` is the "standard distribution" image. Upon booting up on a system where Glidix is not installed, or a broken Glidix installation is present, it will automatically format the hard drive, install a "basic system", and request a reboot. When booting up the second time, the full OS will be installed, and when you reboot once again, the system is ready to use.

Currently, even though the OS is installed on the hard drive, you must still boot from the CD to get the system up. This is because Glidix is not developed enough to support `grub-install` yet.

On the default distribution OS, the Glidix shell and the shell utilities (`shutils`) are all installed. Currently, no programming environment is available (this is coming soon). The CD is mounted under `/media/cdrom` (as specified in `/etc/fstab`). To force a full reinstallation of the OS, just delete the shell using `rm /bin/sh` and `reboot`. This will install everything again, starting with the initial format.
