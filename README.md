Glidix
======

Glidix is an operating system for the x86_64 platform. The custom-made kernel and C library work together to implement the POSIX API, while cleaning up or removing the messy parts, and adding more high-level frameworks on top of that. It is a modernized, graphical operating system which tries to follow UNIX principles to a reasonable degree. It is designed to be easy to use for users, and easy to program for for developers.

## Building

The easiest way to build the OS is to run `./build-dist.sh` in the root directory. Note that it can be quite slow the first time you run it, as it will have to build the cross-compiler and then also build the native compiler. After that it should be a lot faster, because once the compiler is built is does not need to be re-built.

`build-dist.sh` performs a contained build, inside the `build` directory, where it will install the cross-compiler and make a sysroot for it, and add it to its own `PATH`. This means you don't have to set up anything else on your machine. The end result of this script is the file `build/diskimg/glidix.img` which is the bootable disk image of Glidix.

## Running

### Bochs

To run the disk image in Bochs after `build-dist.sh`, just run `bochs` in the root directory as `bochsrc` is already set up.

## Status

The operating system is currently in _alpha_ state. It is currently therefore _not_ guaranteed to be ABI-compatible between versions. In fact, the ABI gets constantly revised, and recompilation is required a lot. We are hoping to move onto a _beta_ version in the next few months, and from there onwards, ABI compatibility will be guaranteed, and binary releases of applications will be possible.

## Requirements

 * x86_64 (aka AMD64) CPU; SSE4.1 support is recommended.
 * 64MB RAM (2GB recommended for GUI).

## Hardware Support

 * Basic hardware: PCI, VGA text mode, PIT, APIC (including timer). This is implemented in the kernel itself; all the things below are implemented as separate modules.
 * Storage devices: IDE and AHCI; IDE is incomplete.
 * Graphics: BGA. Hardware acceleration for various cards is planned.
 * Input devices: PS/2 mouse and keyboard
 * Ethernet cards: NE2000 (promiscous only), Intel 8254x (*PRO/1000 MT Desktop*, and *T/MT Server*), AMD PCNet (promiscous only).

## Features

 * Capable of booting via BIOS from a GPT using a custom bootloader; UEFI boot planned.
 * A simple graphical interface, currently under heavy development.
 * Network stack supporting IPv4 and IPv6, ICMP to some degree, UDP, DNS and TCP. Supports autoconfiguration with DHCP, ALIC (*Automatic Local IP Configuration*, for IPv4 link-local addresses when DHCP is unavailable), and SLAAC for IPv6.
 * Supports the ISO 9660 filesystem, FAT32, and a custom filesystem called Glidix File System (GXFS), which can store Glidix-specific metadata such as application permissions.
 * Implements the POSIX API using a custom C library.
 * The GNU toolchain (binutils and GCC) can run natively on Glidix (the source code must be patched to support the `x86_64-glidix` target).

## GCC

The source comes with GCC and binutils patches, and allows a cross-compiler to be built, targetting Glidix. A native compiler can also be compiled from this. The Glidix target for GCC (`x86_64-glidix`) supports some additional command-line options:

 * `-mconsole` - link an executable as a console application (the default; really a no-op).
 * `-mgui` - link an executable as a GUI application. This is done by linking against `crtgui.o`, which adds the appropriate annotation to the `.glidix.annot` section. By default, it also links against `libgwm` and `libddi`; to prevent this, use `-mnoguilibs`.
 * `-module` - link as a Glidix kernel module. This avoids linking with `libc` and other system libraries, and outputs an object file (typically with `.gkm` extension), which can be loaded by the Glidix kernel.
 * `-mnoguilibs` - when used with `-mgui`, do not link against GUI libraries.

## Planned features

 * USB support.
 * Booting from UEFI.
 * Hardware-accelerated graphics.
 * More drivers and porting more applications.
 * Audio support.