# build-dist stage for building the cross-tools stage 2.

# Build the first stage if the C++ library does not yet exist.
if [ ! -e $DIST_SYSROOT/cross/x86_64-glidix/lib/libstdc++.a ]
then
	log_info "Building libc for crosstools stage 2..."
	cd iso || exit 1
	make libc || exit 1
	DESTDIR=$DIST_SYSROOT make install-libc || exit 1
	cd .. || exit 1

	log_info "Building crosstools stage 2..."
	cd crosstools || exit 1
	./install-stage2.sh || exit 1
	cd .. || exit 1
fi

log_success "Crosstools stage 2 built successfully."