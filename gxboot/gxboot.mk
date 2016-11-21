TOOLS_CFLAGS := -Wall -Werror -L../libc
GXBOOT_CFLAGS := -Wall -Werror -m32 -ffreestanding -fno-builtin
GXBOOT_LDFLAGS := -T $(SRCDIR)/vbr.ld -m32 -ffreestanding -nostdlib -lgcc

C_SRC := $(shell find $(SRCDIR)/vbr -name '*.c')
ASM_SRC := $(shell find $(SRCDIR)/vbr -name '*.asm')
OBJ := $(patsubst $(SRCDIR)/vbr/%.c, vbr/%.o, $(C_SRC)) $(patsubst $(SRCDIR)/vbr/%.asm, asm/%.o, $(ASM_SRC))
DEP := $(OBJ:.o=.d)

.PHONY: all install
all: mbr.bin gxboot-install vbr.bin

mbr.bin: $(SRCDIR)/mbr.asm
	nasm -fbin -o $@ $<

-include $(DEP)

gxboot-install: $(SRCDIR)/gxboot-install.c
	$(HOST_GCC) $< -o $@ $(TOOLS_CFLAGS)

install:
	@mkdir -p $(DESTDIR)/usr/bin
	@mkdir -p $(DESTDIR)/usr/share/gxboot
	cp mbr.bin $(DESTDIR)/usr/share/gxboot/mbr.bin
	cp gxboot-install $(DESTDIR)/usr/bin/gxboot-install
	cp vbr.bin $(DESTDIR)/usr/share/gxboot/vbr.bin

vbr.bin: $(OBJ)
	$(HOST_GCC) -o $@ $^ $(GXBOOT_LDFLAGS)

vbr/%.o: $(SRCDIR)/vbr/%.c
	@mkdir -p vbr
	$(HOST_GCC) -c $< -o $@ $(GXBOOT_CFLAGS)

asm/%.o: $(SRCDIR)/vbr/%.asm
	@mkdir -p asm
	nasm -felf32 -o $@ $<

vbr/%.d: $(SRCDIR)/vbr/%.c
	@mkdir -p vbr
	$(HOST_GCC) -c $< -MM -MT $(subst .d,.o,$@) -o $@ $(CFLAGS)

