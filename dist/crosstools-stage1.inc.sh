# build-dist stage for building the cross-tools stage 1.

# Create the workspace if it does not yet exist.
if [ ! -e crosstools ]
then
	log_info "Settings up the crosstools workspace..."
	mkdir crosstools || exit 1
	cd crosstools || exit 1
	../../setup-crosstools-workspace --sysroot=$DIST_SYSROOT || exit 1
	cd .. || exit 1
fi

# Build the first stage if GCC does not yet exist.
if [ ! -e $DIST_CROSS_BIN_DIR/x86_64-glidix-gcc ]
then
	log_info "Building crosstools stage 1..."
	cd crosstools || exit 1
	./install.sh || exit 1
	cd .. || exit 1
fi

log_success "Crosstools stage 1 built successfully."