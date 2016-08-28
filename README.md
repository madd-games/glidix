Glidix
======

Glidix is an experimental UNIX-clone OS, designed to run on x86_64. It is made mainly for experiments, and is designed to be easy to build and run on 64-bit x86-family processors. We are aiming to make it very customizable and therefore optimizeable for anyone's needs.

## Requirements

 * x86_64 (aka AMD64) CPU; SSE4.1 support is recommended.
 * 64MB RAM (2GB recommended for GUI).

## Hardware Support

 * Basic hardware: PCI, VGA text mode, PIT, APIC (including timer). This is implemented in the kernel itself; all the things below are implemented as separate modules.
 * Storage devices: IDE and AHCI; with AHCI being limited to ATA only (no ATAPI support yet); IDE seems to break when writing.
 * Graphics: BGA. Hardware acceleration for various cards is planned.
 * Input devices: PS/2 mouse and keyboard
 * Ethernet cards: NE2000 (promiscous only), Intel 8254x (only PRO/1000 MT Desktop is listed in the driver as supported but it'd probably work for the other ones too), and broken support for VirtIO net.

## Features

 * Capable of booting from an MBR using a custom bootloader; UEFI boot planned.
 * A simple graphical interface, currently under heavy development.
 * Network stack supporting IPv4 and IPv6, ICMP to some degree, UDP, DNS and also TCP in progress. Supports autoconfiguration with DHCP, ALIC (*Automatic Local IP Configuration*, for IPv4 link-local addresses when DHCP is unavailable), and SLAAC for IPv6.
 * Supports the ISO 9660 filesystem, plus a custom filesystem called Glidix File System (GXFS), which can store Glidix-specific metadata such as application permissions. Support for variants of FAT is planned.
 * Implements the POSIX API using a custom C library.
 * The GNU toolchain (binutils and GCC) can run natively on Glidix (the source code must be patched to support the `x86_64-glidix` target).

## Planned features

 * FAT filesystem support.
 * USB support.
 * Booting from UEFI, and supporting GPT.
 * Hardware-accelerated graphics.
 * More drivers and porting more applications.
