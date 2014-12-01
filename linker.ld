OUTPUT_FORMAT("binary")
ENTRY(_start)
phys = 0x00010000;
SECTIONS
{
  .text phys : AT(phys) {
    code = .;
    *(.bootstrap32)
    *(.text)
    *(.rodata)
    . = ALIGN(4096);
  }
  .data : AT(phys + (data - code))
  {
    data = .;
    *(.data)
    . = ALIGN(4096);
  }
  .bss : AT(phys + (bss - code))
  {
    bss = .;
    *(.bss)
    . = ALIGN(4096);
  }
  end = .;
}
