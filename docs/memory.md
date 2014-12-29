Glidix Memory Map
=================

Glidix uses 4KB pages. Using the PML4, the 48-bit virtual address space is divided into 512GB blocks, each with a different purpose. Here is the specific memory map:

 * `pml4[0]` (except the first page) is for userspace processes.
 * `pml4[256]` is the static kernel image (code, data, bss, symbols, etc).
 * `pml4[257]` is the Interprocess Streaming Page (ISP), this is a single page that gets remapped to different addresses to send data to different address spaces etc. There may be more things in this PML4e in the future.
 * `pml4[258]` is the kernel heap.
 * `pml4[259]` is module space; each module is assigned a 1GB block within this PML4e, which limits the number of modules to 512.

The rest of the address space is reserved.
