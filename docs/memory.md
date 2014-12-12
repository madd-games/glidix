Glidix Memory Map
=================

Glidix uses 4KB pages. Using the PML4, the 48-bit virtual address space is divided into 512GB blocks, each with a different purpose. Here is the specific memory map:

 * `0x0000000000000000`-`0x0000007FFFFFFFFF` - Userspace code and data, bss etc.
 * `0x0000008000000000`-`0x000000FFFFFFFFFF` - The kernel code, data, etc.
 * `0x0000010000000000`-`0x0000017FFFFFFFFF` - The kernel heap. Inaccessible to userspace code.
 * `0x0000018000000000`-`0x0000018000001000` - The Interprocess Streaming Page (ISP), this is a single page that gets remapped to different adresses to send data to different address spaces etc.
