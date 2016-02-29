.PHONY: all
all: mbr.bin stage2.bin
	
mbr.bin: mbr.asm
	nasm -f bin -o mbr.bin mbr.asm

stage2.bin: stage2.asm display.asm gxfs.asm
	nasm -f bin -o stage2.bin stage2.asm
