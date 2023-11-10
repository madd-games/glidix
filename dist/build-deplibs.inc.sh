# build-dist stage to build the libraries that the OS depends on and install them in the sysroot.

log_info "Building dependency libraries and installing in the sysroot..."
cd iso || exit 1

make libc
DESTDIR=$DIST_SYSROOT make install-libc
make libz
DESTDIR=$DIST_SYSROOT make install-libz
make libpng
DESTDIR=$DIST_SYSROOT make install-libpng
make libgpm
DESTDIR=$DIST_SYSROOT make install-libgpm
make freetype
DESTDIR=$DIST_SYSROOT make install-freetype
make libddi
DESTDIR=$DIST_SYSROOT make install-libddi
make fstools
DESTDIR=$DIST_SYSROOT make install-fstools
make libgwm
DESTDIR=$DIST_SYSROOT make install-libgwm

cd .. || exit 1

log_success "Dependency libraries built successfully."