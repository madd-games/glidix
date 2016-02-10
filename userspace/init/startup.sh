echo "Mounting all filesystems..."
mount -a
echo "Starting services"
service state 2
echo "Starting login manager..."
logmgr
