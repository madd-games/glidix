# build-dist stage for configuring the diskimg build.

# Create the build directory if it does not yet exist.
if [ ! -e diskimg ]
then
	log_info "Configuring the distro build..."
	mkdir diskimg || exit 1
	cd diskimg || exit 1
	../../configure --sysroot=$DIST_SYSROOT --diskimg || exit 1
	cd .. || exit 1
fi