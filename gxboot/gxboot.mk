TOOLS_CFLAGS := -Wall -Werror -L../libc
GXBOOT_CFLAGS := -Wall -Werror -m32 -ffreestanding -fno-builtin -mno-mmx -mno-sse2 -mno-sse
GXBOOT_LDFLAGS := -T $(SRCDIR)/vbr.ld -m32 -ffreestanding -nostdlib -lgcc

C_SRC := $(shell find $(SRCDIR)/vbr -name '*.c')
ASM_SRC := $(shell find $(SRCDIR)/vbr -name '*.asm')
OBJ := $(patsubst $(SRCDIR)/vbr/%.c, vbr/%.o, $(C_SRC)) $(patsubst $(SRCDIR)/vbr/%.asm, asm/%.o, $(ASM_SRC))
OBJ_ELT := $(patsubst $(SRCDIR)/vbr/%.c, vbr-elt/%.o, $(C_SRC)) $(patsubst $(SRCDIR)/vbr/%.asm, asm-elt/%.o, $(ASM_SRC))
DEP := $(OBJ:.o=.d) $(OBJ_ELT:.o=.d)

.PHONY: all install
all: mbr.bin gxboot-install vbr.bin eltorito-stage1.bin vbr-elt.bin eltorito.img

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
	$(HOST_GCC) -c $< -o $@ $(GXBOOT_CFLAGS) -DGXBOOT_FS_GXFS

asm/%.o: $(SRCDIR)/vbr/%.asm stub64.bin
	@mkdir -p asm
	nasm -felf32 -o $@ $< -DGXBOOT_FS_GXFS

vbr/%.d: $(SRCDIR)/vbr/%.c
	@mkdir -p vbr
	$(HOST_GCC) -c $< -MM -MT $(subst .d,.o,$@) -o $@ $(GXBOOT_CFLAGS) -DGXBOOT_FS_GXFS

vbr-elt.bin: $(OBJ_ELT)
	$(HOST_GCC) -o $@ $^ $(GXBOOT_LDFLAGS)

vbr-elt/%.o: $(SRCDIR)/vbr/%.c
	@mkdir -p vbr-elt
	$(HOST_GCC) -c $< -o $@ $(GXBOOT_CFLAGS) -DGXBOOT_FS_ELTORITO

asm-elt/%.o: $(SRCDIR)/vbr/%.asm stub64.bin
	@mkdir -p asm-elt
	nasm -felf32 -o $@ $< -DGXBOOT_FS_ELTORITO

vbr-elt/%.d: $(SRCDIR)/vbr/%.c
	@mkdir -p vbr-elt
	$(HOST_GCC) -c $< -MM -MT $(subst .d,.o,$@) -o $@ $(GXBOOT_CFLAGS) -DGXBOOT_FS_ELTORITO

eltorito-stage1.bin: $(SRCDIR)/eltorito-stage1.asm
	nasm -fbin -o elt-temp.bin $<
	cp elt-temp.bin $@
	truncate -s2048 $@

eltorito.img: eltorito-stage1.bin vbr-elt.bin
	cat eltorito-stage1.bin vbr-elt.bin > $@

# NOTE: The file must have the *.asmb extension because asm is compiled to ELF32!
stub64.bin: $(SRCDIR)/vbr/stub64.asmb
	nasm -fbin -o $@ $<
