ENTRY(_start)

LOADER_BASE = 0xE00000;
STACK_SIZE = 0x4000;

SECTIONS
{
	. = LOADER_BASE;
	
	.text :
	{
		code = .;
		*(.multiboot)
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
	
	. = ALIGN(4);
	. += STACK_SIZE;
	
	stack = .;
}
