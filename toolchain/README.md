Glidix Toolchain Patches
========================

These patches are made for binutils 2.24 and GCC 4.9.0, there is no guarantee they will work with other versions. To use them, download binutils 2.24, put `glidix-binutils-2.24.patch` in the `binutils-2.24` directory, then run:

`patch -p1 < glidix-binutils-2.24.patch`

Then do the same for GCC 4.9.0. This will patch the source code such that they can target `x86_64-glidix`, and you can use them to build the kernel, system libraries, and any Glidix userspace programs.

To build binutils for cross-compilation, configure and build as follows:

`../binutils-2.24/configure --target=x86_64-glidix --prefix=/glidix/usr --with-sysroot=/glidix --disable-werror`  
`make`  
`make install`  

To build gcc for cross-compilation, configure and build as follows:

`../gcc-4.9.0/configure --target=x86_64-glidix --prefix=/glidix/usr --with-sysroot=/glidix --enable-languages=c,c++`  
`make all-gcc all-target-libgcc`  
`make install-gcc install-target-libgcc`  
