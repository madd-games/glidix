OUTPUT_FORMAT("binary")
ENTRY(_start)
phys = 0xFFFF800000100000;

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
		_kernel_symtab = .;
		*(.symtab)
		_kernel_symtab_end = .;
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
