# build-dist stage for building the actual disk image.

log_info "Building the distro disk image..."
cd diskimg || exit 1

if command -v bear >/dev/null 2>&1; then
	log_info '`bear` detected; compile_commands.json will be generated/updated during the build'
	bear --output ../compile_commands.json --append -- make || exit 1
else
	make || exit 1
fi

cd .. || exit 1