.PHONY: all
all: mbload grub.cfg

mbload: loader.o loader_s.o
	$(HOST_GCC) -m32 -T $(SRCDIR)/loader.ld -o $@ $^ -lgcc -nostdlib -ffreestanding

loader.o: $(SRCDIR)/loader.c
	$(HOST_GCC) -m32 -c $< -o $@ -Wall -Werror -fno-builtin -ffreestanding

loader_s.o: $(SRCDIR)/loader.asm stub64.bin
	nasm -felf32 -o loader_s.o $(SRCDIR)/loader.asm

stub64.bin: $(SRCDIR)/stub64.asm
	nasm -fbin -o stub64.bin $(SRCDIR)/stub64.asm

grub.cfg:
	echo >$@ 'menuentry "Glidix $(GLIDIX_VERSION) (mbload)" {'
	echo >>$@ '	multiboot /boot/mbload'
	echo >>$@ '	module /boot/vmglidix.tar'
	echo >>$@ '}'

.PHONY: install
install:
	@mkdir -p $(DESTDIR)/boot/grub
	cp grub.cfg $(DESTDIR)/boot/grub/grub.cfg
	cp mbload $(DESTDIR)/boot/mbload
