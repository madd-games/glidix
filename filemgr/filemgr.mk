SRC := $(shell find $(SRCDIR)/src -name '*.c')
OBJ := $(patsubst $(SRCDIR)/%.c, obj/%.o, $(SRC))
DEP := $(OBJ:.o=.d)
CFLAGS := -Wall -Werror -I$(SRCDIR)/../libgwm -I$(SRCDIR)/../libddi -I$(SRCDIR)/../fstools/lib -ggdb

.PHONY: install

filemgr: $(OBJ)
	$(HOST_GCC) $^ -o $@ -L../libgwm -L../libddi -L../fstools -mgui -lfstools -ggdb

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
	@mkdir -p $(DESTDIR)/etc/filemgr/openwith
	cp filemgr $(DESTDIR)/usr/bin/filemgr
	cp -RT $(SRCDIR)/apps $(DESTDIR)/usr/share/apps
	cp -RT $(SRCDIR)/openwith $(DESTDIR)/etc/filemgr/openwith
