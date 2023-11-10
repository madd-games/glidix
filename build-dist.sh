#! /bin/bash

# Useful functions for the stages.
CSI="\e["
RESET="${CSI}0m"
BOLD="${CSI}1m"
BLUE="${CSI}34m"
RED="${CSI}31m"
YELLOW="${CSI}33m"
GREEN="${CSI}32m"

log_info() {
    printf "${BOLD}${BLUE}[build-dist] [info] ${RESET}${BLUE}%s${RESET}\n" "$1"
}

log_error() {
    printf "${BOLD}${RED}[build-dist] [error] ${RESET}${RED}%s${RESET}\n" "$1"
}

log_warn() {
    printf "${BOLD}${YELLOW}[build-dist] [warn] ${RESET}${YELLOW}%s${RESET}\n" "$1"
}

log_success() {
    printf "${BOLD}${GREEN}[build-dist] [success] ${RESET}${GREEN}%s${RESET}\n" "$1"
}

log_info "Preparing the distro build..."

# Compute the paths required by other scripts.
export GLIDIX_SRC_DIR="`realpath .`"
export DIST_BUILD_DIR="$GLIDIX_SRC_DIR/build"
export DIST_SYSROOT="$DIST_BUILD_DIR/sysroot"
export DIST_CROSS_BIN_DIR="$DIST_SYSROOT/cross/bin"

# Add the cross-compiler to the PATH.
export PATH="$PATH:$DIST_CROSS_BIN_DIR"

# Create and go into the build directory.
mkdir -p build || exit 1
cd build || exit 1

# Run the build stages.
. ../dist/crosstools-stage1.inc.sh
. ../dist/configure-iso-build.inc.sh
. ../dist/crosstools-stage2.inc.sh
