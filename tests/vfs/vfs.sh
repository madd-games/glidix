# vfs.sh
# Prototype the VFS in userspace
testdir="`dirname $0`"
srcdir="$testdir/../.."

# Attempt to compile the test
command="cc -I$srcdir/kernel/include $srcdir/kernel/src/vfs.c $testdir/vfs-test.c -o vfs-test $TEST_CFLAGS -ggdb -w"
echo "Compiling VFS unit test using: $command"
$command || exit 1

# Now run it
echo "Running VFS unit test:"
./vfs-test || exit 1

echo "VFS unit test is OK"
