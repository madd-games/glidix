SRC := $(shell find $(SRCDIR)/src -name '*.c')
OBJ := $(patsubst $(SRCDIR)/%.c, obj/%.o, $(SRC))
DEP := $(OBJ:.o=.d)
CFLAGS := -Wall -Werror -I$(SRCDIR)/../libgwm -I$(SRCDIR)/../libddi

.PHONY: install

sysinfo: $(OBJ)
	$(HOST_GCC) $^ -o $@ -L../libgwm -L../libddi -lddi -lgwm

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
	cp sysinfo $(DESTDIR)/usr/bin/sysinfo
	cp -RT $(SRCDIR)/apps $(DESTDIR)/usr/share/apps
	cp -RT $(SRCDIR)/images $(DESTDIR)/usr/share/images
