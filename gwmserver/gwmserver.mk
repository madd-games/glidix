SRC := $(shell find $(SRCDIR)/src -name '*.c')
OBJ := $(patsubst $(SRCDIR)/%.c, obj/%.o, $(SRC))
DEP := $(OBJ:.o=.d)
CFLAGS := -Wall -Werror -I$(SRCDIR)/../libgwm -I$(SRCDIR)/../libddi -ggdb -I$(SRCDIR)/../kernel/include -D_GLIDIX_SOURCE

.PHONY: install

gui: $(OBJ)
	$(HOST_GCC) $^ -o $@ -L../libgwm -L../libddi -L../fstools -mgui -ldl -ggdb

-include $(DEP)

obj/%.d: $(SRCDIR)/%.c
	@mkdir -p $(dir $@)
	$(HOST_GCC) -c $< -MM -MT $(subst .d,.o,$@) -o $@ $(CFLAGS)

obj/%.o: $(SRCDIR)/%.c
	@mkdir -p $(dir $@)
	$(HOST_GCC) -c $< -o $@ $(CFLAGS)

install:
	@mkdir -p $(DESTDIR)/usr/bin
	@mkdir -p $(DESTDIR)/usr/share/kblayout
	cp gui $(DESTDIR)/usr/bin/gui
	cp -RT $(SRCDIR)/kblayout $(DESTDIR)/usr/share/kblayout
