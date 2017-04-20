ENTRY(_start)

SECTIONS
{
	.modbody 0 : AT(0) {
		_start = .;
		*(.init)
		*(.text)
		_fini = .;
		*(.fini)
		*(.rodata)
		*(.data)
		*(.bss)
		__modend = .;
	}
}
