SRC := $(shell find $(SRCDIR)/src -name '*.c')
OBJ := $(patsubst $(SRCDIR)/%.c, obj/%.o, $(SRC))
DEP := $(OBJ:.o=.d)
CFLAGS := -Wall -Werror -ggdb -D_GLIDIX_SOURCE

.PHONY: install

gpm: $(OBJ)
	$(HOST_GCC) $^ -o $@ -ggdb

-include $(DEP)

obj/%.d: $(SRCDIR)/%.c
	@mkdir -p $(dir $@)
	$(HOST_GCC) -c $< -MM -MT $(subst .d,.o,$@) -o $@ $(CFLAGS)

obj/%.o: $(SRCDIR)/%.c
	@mkdir -p $(dir $@)
	$(HOST_GCC) -c $< -o $@ $(CFLAGS)

install:
	@mkdir -p $(DESTDIR)/usr/bin
	cp gpm $(DESTDIR)/usr/bin/gpm
