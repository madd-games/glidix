C_SRC := $(shell find $(SRCDIR)/src -name '*.c')
ASM_SRC := $(shell find $(SRCDIR)/asm -name '*.asm')
OBJ := $(patsubst $(SRCDIR)/%.c, obj/%.o, $(C_SRC)) $(patsubst $(SRCDIR)/asm/%.asm, asm/%.o, $(ASM_SRC))
DEP := $(OBJ:.o=.d)
CFLAGS := -ffreestanding -mcmodel=large -mno-red-zone -mno-mmx -mno-sse -mno-sse2 -fno-common -fno-builtin -fno-omit-frame-pointer -I $(SRCDIR)/include -I $(SRCDIR)/include/acpi -I. -D__KERNEL__ -DCONFIG_ACPI -Wall -Werror

.PHONY: all
all: out/kernel.so module_start.o module_end.o

out/kernel.so: $(OBJ)
	@mkdir -p out
	$(HOST_GCC) -T $(SRCDIR)/kernel.ld -o $@ -ffreestanding -O2 -nostdlib $^ -lgcc

-include $(DEP)

obj/%.d: $(SRCDIR)/%.c
	@mkdir -p $(dir $@)
	$(HOST_GCC) -c $< -MM -MT $(subst .d,.o,$@) -o $@ $(CFLAGS)

obj/%.o: $(SRCDIR)/%.c
	@mkdir -p $(dir $@)
	$(HOST_GCC) -c $< -o $@ $(CFLAGS)

asm/%.o: $(SRCDIR)/asm/%.asm
	@mkdir -p asm
	nasm -felf64 -o $@ $<

module_start.o: $(SRCDIR)/module/module_start.asm
	nasm -felf64 -o $@ $<

module_end.o: $(SRCDIR)/module/module_end.asm
	nasm -felf64 -o $@ $<

