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
		end = .;
		/*. = 0xFFFF828000000000;
		_per_cpu_start = .;
		*(.data_per_cpu)
		_per_cpu_end = .*/;
	}
}
