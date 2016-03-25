OUTPUT_FORMAT("binary")
ENTRY(_start)
phys = 0x7C00;

SECTIONS
{
	.text phys : AT(phys) {
		code = .;
		*(.glue16)
		*(.text)
		*(.rodata)
	}

	.data (phys + (data - code)) : AT(phys + (data - code))
	{
		data = .;
		*(.data)
	}

	.bss (phys + (bss - code)) : AT(phys + (bss - code))
	{
		bss = .;
		*(.bss)
	}
	
	end = .;
}
