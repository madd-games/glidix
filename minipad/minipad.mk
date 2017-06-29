SRC := $(shell find $(SRCDIR)/src -name '*.c')
OBJ := $(patsubst $(SRCDIR)/%.c, obj/%.o, $(SRC))
DEP := $(OBJ:.o=.d)
CFLAGS := -Wall -Werror -I$(SRCDIR)/../libgwm -I$(SRCDIR)/../libddi -ggdb

.PHONY: install

minipad: $(OBJ)
	$(HOST_GCC) $^ -o $@ -L../libgwm -L../libddi -L../fstools -mgui -ggdb

-include $(DEP)

obj/%.d: $(SRCDIR)/%.c
	@mkdir -p $(dir $@)
	$(HOST_GCC) -c $< -MM -MT $(subst .d,.o,$@) -o $@ $(CFLAGS)

obj/%.o: $(SRCDIR)/%.c
	@mkdir -p $(dir $@)
	$(HOST_GCC) -c $< -o $@ $(CFLAGS)

install:
	@mkdir -p $(DESTDIR)/usr/bin
	@mkdir -p $(DESTDIR)/usr/share/apps
	@mkdir -p $(DESTDIR)/usr/share/images
	cp minipad $(DESTDIR)/usr/bin/minipad
	cp -RT $(SRCDIR)/apps $(DESTDIR)/usr/share/apps
	cp -RT $(SRCDIR)/images $(DESTDIR)/usr/share/images
