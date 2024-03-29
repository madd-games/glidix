# Syntax: make-diskimg.sh <imgdir> <diskimg_file>
#
# Creates a bootable disk image with the specified directory being copied as the root
# filesystem.

DISKIMG_EFISYS_MB="128"
DISKIMG_DEFAULT_SIZE="6G"

# Get arguments.
imgdir="$1"
diskimg_file="$2"

# Delete the image if it already exists, and create it with the default size.
rm -f $diskimg_file || exit 1
fallocate -l $DISKIMG_DEFAULT_SIZE $diskimg_file || exit 1

# Create the partition table on the image.
build-tools/mkgpt $diskimg_file \
	--part type=efisys size=$DISKIMG_EFISYS_MB name='EFI System Partition' \
	--part type=gxfs-root name='Glidix Installer' \
|| exit 1

# Format the GXFS root partition and put the image files on it.
build-tools/mkrootimg $diskimg_file $imgdir || exit 1

# Install the bootloader on the image.
GXBOOT_IMAGE_PATH=gxboot build-tools/gxboot-install $diskimg_file || exit 1

# If we need to, create the VMDK file.
if command -v VBoxManage >/dev/null 2>&1 && [ ! -e glidix.vmdk ]
then
	VBoxManage internalcommands createrawvmdk -filename glidix.vmdk -rawdisk $diskimg_file
fi