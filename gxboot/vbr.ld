/* link the VBR as a flat binary */
OUTPUT_FORMAT("binary")
ENTRY(_start)
phys = 0x7C00;

SECTIONS
{
	code = .;

	.text phys : AT(phys) {
		*(.entry_text)
		*(.text)
		*(.rodata)
		*(.rodata*)
		*(.eh_frame)
	}

	data = .;
	.data (phys + (data - code)) : AT(phys + (data - code))
	{
		*(.data)
	}

	bss = .;
	.bss (phys + (bss - code)) : AT(phys + (bss - code))
	{
		*(.bss)
	}
	
	vbr_end = .;
}
