#! /bin/sh
# This script is run in the /usr/libexec directory after shutils.mip is unpacked.
# ALL PATHS MUST BE RELATIVE.

echo "[shutils] setting up permissions for shutils..."
chxperm ../bin/sudo own.all=1 delegate.all=1
chxperm ../bin/chxperm inherit.chxperm=1 own.all=0 delegate.all=1
chxperm ../bin/ping own.rawsock=1
chxperm ../bin/ping6 own.rawsock=1
chxperm ../bin/env inherit.all=1 delegate.all=1
chxperm ../bin/mip-install inherit.all=1 delegate.all=1

chmod 6755 ../bin/sudo

# Links
ln ../bin/halt ../bin/poweroff
ln ../bin/halt ../bin/reboot
