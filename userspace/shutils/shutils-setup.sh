#! /bin/sh
# This script is run in the /usr/libexec directory after shutils.mip is unpacked.
# ALL PATHS MUST BE RELATIVE.

echo "[shutils] setting up permissions for shutils..."
chxperm /mnt/usr/bin/sudo own.all=1 delegate.all=1
chxperm /mnt/usr/bin/chxperm inherit.chxperm=1 own.all=0 delegate.all=1
chxperm /mnt/usr/bin/ping own.rawsock=1
