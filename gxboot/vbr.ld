/* link the VBR as a flat binary */
OUTPUT_FORMAT("binary")
ENTRY(_start)
phys = 0x7C00;

SECTIONS
{
	.text phys : AT(phys) {
		code = .;
		*(.entry_text)
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
	
	vbr_end = .;
}
