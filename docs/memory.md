Glidix Memory Map
=================

Glidix uses 4KB pages. Using the PML4, the 48-bit virtual address space is divided into 512GB blocks, each with a different purpose. Here is the specific memory map:

 * `0x0000000000000000`-`0x0000007FFFFFFFFF` - kernel code and data, and the bootstrap stack. Inaccessible to userspace code (not even readable).
 * `0x0000008000000000`-`0x000000FFFFFFFFFF` - Userspace code, data, stack, etc. All of the userspace code is here. There is also a kernel stack area which is used upon interrupts. This is the only 512GB block which gets changed between each process.
 * `0x0000010000000000`-`0x0000017FFFFFFFFF` - The kernel heap. Inaccessible to userspace code.
 * `0x18000000000`-`0x18000001000` - The Interprocess Streaming Page (ISP), this is a single page that gets remapped to different adresses to send data to different address spaces etc.
