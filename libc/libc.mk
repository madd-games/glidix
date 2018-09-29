C_SRC := $(shell find $(SRCDIR)/src -name '*.c')
ASM_SRC := $(shell find $(SRCDIR)/asm -name '*.s')
ASM_STATIC := $(shell find $(SRCDIR)/sasm -name '*.s')

OBJ_DYN := $(patsubst $(SRCDIR)/%.c, obj/%.o, $(C_SRC)) $(patsubst $(SRCDIR)/asm/%.s, asm/%.o, $(ASM_SRC))
OBJ_STATIC := $(patsubst $(SRCDIR)/%.c, sobj/%.o, $(C_SRC)) $(patsubst $(SRCDIR)/asm/%.s, asm/%.o, $(ASM_SRC)) $(patsubst $(SRCDIR)/sasm/%.s, sasm/%.o, $(ASM_STATIC))

DEP_DYN := $(OBJ_DYN:.o=.d)
DEP_STATIC := $(OBJ_STATIC:.o=.d)

DEP := $(DEP_DYN) $(DEP_STATIC)

CFLAGS := -fPIC -I $(SRCDIR)/include -Wall -Werror -D_GLIDIX_SOURCE -ggdb
CFLAGS_STATIC := -I $(SRCDIR)/include -Wall -Werror -D_GLIDIX_SOURCE -ggdb

SUP_SRC := $(shell find $(SRCDIR)/support -name '*.c')
SUP_OUT := $(patsubst $(SRCDIR)/support/%.c, support/%.so, $(SUP_SRC))

CRT_SRC := $(shell find $(SRCDIR)/crt -name '*.s')
CRT_OUT := $(patsubst $(SRCDIR)/crt/%.s, crt/%.o, $(CRT_SRC))

.PHONY: all install
all: libc.so libc.a libc32.a $(SUP_OUT) $(CRT_OUT)

libc.so: $(OBJ_DYN)
	$(HOST_GCC) -shared -o $@ $^ -nostdlib -lgcc

libc.a: $(OBJ_STATIC)
	rm -f libc.a
	$(HOST_AR) rc $@ $^
	$(HOST_RANLIB) $@

libc32.a: $(SRCDIR)/libc32.s
	rm -f libc32.a
	$(HOST_AS) -c $< -o libc32.o --32
	$(HOST_AR) rc $@ libc32.o
	$(HOST_RANLIB) $@

-include $(DEP)

obj/%.d: $(SRCDIR)/%.c
	@mkdir -p $(dir $@)
	$(HOST_GCC) -c $< -MM -MT $(subst .d,.o,$@) -o $@ $(CFLAGS)

sobj/%.d: $(SRCDIR)/%.c
	@mkdir -p $(dir $@)
	$(HOST_GCC) -c $< -MM -MT $(subst .d,.o,$@) -o $@ $(CFLAGS)

obj/%.o: $(SRCDIR)/%.c
	@mkdir -p $(dir $@)
	$(HOST_GCC) -c $< -o $@ $(CFLAGS)

sobj/%.o: $(SRCDIR)/%.c
	@mkdir -p $(dir $@)
	$(HOST_GCC) -c $< -o $@ $(CFLAGS_STATIC)

asm/%.o: $(SRCDIR)/asm/%.s
	@mkdir -p asm
	$(HOST_AS) -c $< -o $@

sasm/%.o: $(SRCDIR)/sasm/%.s
	@mkdir -p sasm
	$(HOST_AS) -c $< -o $@

support/%.so: $(SRCDIR)/support/%.c
	@mkdir -p support
	$(HOST_GCC) -shared $< -o $@ $(CFLAGS) -L. -ggdb

crt/%.o: $(SRCDIR)/crt/%.s
	@mkdir -p crt
	$(HOST_AS) -c $< -o $@

install:
	mkdir -p $(DESTDIR)/lib
	mkdir -p $(DESTDIR)/lib32
	mkdir -p $(DESTDIR)/usr/include
	cp libc.so $(DESTDIR)/lib/libc.so
	cp libc.a $(DESTDIR)/lib/libc.a
	cp libc32.a $(DESTDIR)/lib32/libc.a
	cp -RT crt $(DESTDIR)/lib
	cp -RT support $(DESTDIR)/lib
	cp -RT $(SRCDIR)/include $(DESTDIR)/usr/include
	ln -sf libc.so $(DESTDIR)/lib/libg.so
	ln -sf libc.a $(DESTDIR)/lib/libg.a
	ln -sf libc.a $(DESTDIR)/lib32/libg.a
