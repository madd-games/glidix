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

	.data (phys + (data - code)) : AT(phys + (data - code))
	{
		data = .;
		*(.data)
		. = ALIGN(4096);
	}

	.bss (phys + (bss - code)) : AT(phys + (bss - code))
	{
		bss = .;
		*(.bss)
		. = ALIGN(4096);
	}

	end = .;
}
