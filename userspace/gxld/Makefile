.PHONY: all
all: mbr.bin stage2.bin
	
mbr.bin: mbr.asm
	nasm -f bin -o mbr.bin mbr.asm

stage2.bin: stage2_wrapper.asm stage2_rm.asm stage2.c
	nasm -fbin -o stage2_rm.bin stage2_rm.asm
	nasm -felf32 -o stage2_wrapper.o stage2_wrapper.asm
	i686-elf-gcc -c stage2.c -o stage2.o -ffreestanding
	nasm -felf32 -o stage2_pm.o stage2_pm.asm
	i686-elf-gcc -T linker.ld -o stage2.bin stage2_wrapper.o stage2.o stage2_pm.o -nostdlib -lgcc
