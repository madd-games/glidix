Glidix Memory Map
=================

Glidix uses 4KB pages. Using the PML4, the 48-bit physical address space is divided into 512GB blocks, each with a different purpose. Here is the specific memory map:

*`0x0000000000000000`-`0x0000008000000000` - kernel code and data, and the bootstrap stack.
