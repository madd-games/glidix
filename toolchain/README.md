Glidix Toolchain Patches
========================

These patches are made for binutils 2.24 and GCC 4.9.0, there is no guarantee they will work with other versions. To use them, download binutils 2.24, put `glidix-binutils-2.24.patch` in the `binutils-2.24` directory, then run:

`patch -p1 < glidix-2.24.patch`

Then do the same for GCC 4.9.0. This will patch the source code such that they can target `x86_64-glidix`, and you can use them to build the kernel, system libraries, and any Glidix userspace programs.
