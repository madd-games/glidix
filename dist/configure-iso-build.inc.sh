# build-dist stage for configuring the ISO build.

# Create the build directory if it does not yet exist.
if [ ! -e iso ]
then
	log_info "Configuring the ISO build..."
	mkdir iso || exit 1
	cd iso || exit 1
	../../configure --sysroot=$DIST_SYSROOT --iso || exit 1
	cd .. || exit 1
fi