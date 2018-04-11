echo "Mounting all filesystems..."
mount -a
echo "Starting level 1 services..."
service state 1
echo "Loading kernel modules..."
ldmods || exit 1
echo "Starting level 2 services..."
service state 2
echo "Starting installer..."
gxsetup
