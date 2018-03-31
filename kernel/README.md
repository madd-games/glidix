Glidix Kernel
=============

The code is sorted into subdirectories based on category. Explanations:

* `hw` contains code that manages low-level hardware which does not fit into other categories (for example ACPI, APIC, etc), and the CPU.
* `display` - managing the display and the kernel console.
* `int` - kernel interface to userspace, such as system calls, loading executables, core dumps, etc.
* `thread` - thread management and syncrhonization primitives.
* `util` - utilities.
* `fs` - everything related to the file system.
* `net` - networking and sockets.
* `humin` - human input devices.
* `term` - terminal stuff.
* `storage` - things related to storage/block devices.
* `module` - things related to modules and other link-related kernel routines, such as symbol tables.
* `usb` - things related to USB.

