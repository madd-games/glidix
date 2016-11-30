ENTRY(_start)

SECTIONS
{
	. = 0x100000000;			/* 4GB */
	
	.text :
	{
		code = .;
		*(.text)
		*(.rodata)
	}
	
	.data :
	{
		data = .;
		*(.data)
	}
	
	.bss :
	{
		bss = .;
		*(.bss)
	}
}
