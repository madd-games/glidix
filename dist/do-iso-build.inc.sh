# build-dist stage for building the actual ISO.

log_info "Building the distro ISO..."
cd iso || exit 1

make || exit 1

log_success "DISTRO BUILD SUCCESSFUL."