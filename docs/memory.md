Glidix Memory Map
=================

Glidix uses 4KB pages. Using the PML4, the 48-bit virtual address space is divided into 512GB blocks, each with a different purpose. Here is the specific memory map:

<<<<<<< HEAD
 * `pml4[0]` (except the first page) is for userspace processes.
 * `pml4[256]` is the static kernel image (code, data, bss, symbols, etc).
 * `pml4[257]` is the Interprocess Streaming Page (ISP), this is a single page that gets remapped to different addresses to send data to different address spaces etc. There may be more things in this PML4e in the future.
 * `pml4[258]` is the kernel heap.

The rest of the address space is reserved.
=======
 * `0x0000000000000000`-`0x0000007FFFFFFFFF` - Userspace code and data, bss etc.
 * `0x0000008000000000`-`0x000000FFFFFFFFFF` - The kernel code, data, etc.
 * `0x0000010000000000`-`0x0000017FFFFFFFFF` - The kernel heap. Inaccessible to userspace code.
 * `0x0000018000000000`-`0x0000018000001000` - The Interprocess Streaming Page (ISP), this is a single page that gets remapped to different adresses to send data to different address spaces etc.
>>>>>>> 4fc0b88442e5e9a5da0ebdf926998612035c7f82
