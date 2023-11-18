# Syntax: make-diskimg.sh <imgdir> <output_file>
#
# Creates a bootable disk image with the specified directory being copied as the root
# filesystem.

DISKIMG_EFISYS_MB="128"
DISKIMG_DEFAULT_SIZE="6G"

# Get arguments.
imgdir="$1"
output_file="$2"

# If the disk image does not yet exist, then create it with a default size,
# and create the partition table.
if [ ! -e $output_file ]
then
	fallocate -l $DISKIMG_DEFAULT_SIZE $output_file || exit 1
	build-tools/mkgpt $output_file \
		--part type=efisys size=$DISKIMG_EFISYS_MB name='EFI System Partition' \
		--part type=gxfs-root name='Glidix Installer' \
	|| exit 1
fi