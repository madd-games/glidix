C_SRC := $(shell find $(SRCDIR)/src -name '*.c')
ASM_SRC := $(shell find $(SRCDIR)/asm -name '*.asm')
OBJ := $(patsubst $(SRCDIR)/%.c, obj/%.o, $(C_SRC)) $(patsubst $(SRCDIR)/asm/%.asm, asm/%.o, $(ASM_SRC))
DEP := $(OBJ:.o=.d)
CFLAGS := -Wall -Werror -Bsymbolic -O2 -fPIC

.PHONY: install

dynld: $(OBJ)
	$(HOST_GCC) -T $(SRCDIR)/dynld.ld -o $@ -Wl,-Bsymbolic -O2 -nostdlib $^ -lgcc

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

install:
	mkdir -p $(DESTDIR)/bin
	cp dynld $(DESTDIR)/bin/dynld
