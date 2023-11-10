/* link the VBR as a flat binary */
OUTPUT_FORMAT("binary")
ENTRY(_start)
phys = 0x7C00;

SECTIONS
{
	. = phys;
	code = .;

	.text . : {
		*(.entry_text)
		*(.text)
		*(.rodata)
		*(.rodata*)
		*(.eh_frame)
	}

	. = ALIGN(8);

	data = .;
	.data . :
	{
		*(.data)
	}

	. = ALIGN(8);

	bss = .;
	.bss . :
	{
		*(.bss)
	}
	
	vbr_end = .;
}
