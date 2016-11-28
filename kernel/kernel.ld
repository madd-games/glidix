KERNEL_VMA = 0xFFFF800000100000;		/* virtual address of kernel */
KERNEL_PHYS = 0x100000;				/* physical address of kernel */

PHDRS
{
	lowmem 0x60000000;			/* tells bootloader to map memory, but not load */
	text PT_LOAD;
	data PT_LOAD;
	videoram 0x60000000;			/* tells bootloader to map memory, but not load */
	initrd 0x60000001;			/* tells bootloader to load the initrd here */
	percpu PT_LOAD;
}

SECTIONS
{
	.lowmem 0xFFFF800000000000 : AT(0)
	{
		*(.lowmem)
	} :lowmem
	
	. = KERNEL_VMA;
	
	.text : AT(code + KERNEL_PHYS - KERNEL_VMA)
	{
		code = .;
		*(.text)
		*(.rodata)
		. = ALIGN(4096);
		*(.usup_text)
	} :text
	
	. = ALIGN(4096);
	
	.data : AT(data + KERNEL_PHYS - KERNEL_VMA)
	{
		data = .;
		*(.data)
	} :data
	
	.bss : AT(bss + KERNEL_PHYS - KERNEL_VMA)
	{
		bss = .;
		*(.bss)
	} :data
	
	. = ALIGN(4096);
	
	.stack (NOLOAD) :
	{
		*(.stack)
	} :data
	_stackBottom = .;
	
	. = ALIGN(4096);
	
	.initrd (NOLOAD) :
	{
		*(.initrd)
	} :initrd

	. = ALIGN(4096);
	
	.videoram (NOLOAD) : AT(0xB8000) ALIGN(0x1000)
	{
		*(.videoram)
	} :videoram

	. = ALIGN(4096);
	per_cpu_phys_start = .;
	. = 0xFFFF828000000000;
	_per_cpu_start = .;
	.per_cpu_bss (NOLOAD) : AT(per_cpu_phys_start + KERNEL_PHYS - KERNEL_VMA)
	{
		*(.data_per_cpu)
	} :percpu
	_per_cpu_end = .;
}
