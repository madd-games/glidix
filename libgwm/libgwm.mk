C_SRC := $(shell find $(SRCDIR) -name '*.c')
OBJ := $(patsubst $(SRCDIR)/%.c, %.o, $(C_SRC))
DEP := $(OBJ:.o=.d)
CFLAGS := -Wall -Werror -fPIC -I$(SRCDIR)/../libddi -I$(SRCDIR) -I$(SRCDIR)/../fstools/lib -ggdb

libgwm.so: $(OBJ)
	@mkdir -p out
	$(HOST_GCC) -shared -o $@ $^ -L../libddi -lddi -L../fstools -lfstools -ggdb

-include $(DEP)

%.d: $(SRCDIR)/%.c
	$(HOST_GCC) -c $< -MM -MT $(subst .d,.o,$@) -o $@ $(CFLAGS)

%.o: $(SRCDIR)/%.c
	$(HOST_GCC) -c $< -o $@ $(CFLAGS)

.PHONY: install
install:
	@mkdir -p $(DESTDIR)/usr/lib
	@mkdir -p $(DESTDIR)/usr/include
	@mkdir -p $(DESTDIR)/usr/share/images
	@mkdir -p $(DESTDIR)/usr/share/fonts
	cp libgwm.so $(DESTDIR)/usr/lib/libgwm.so
	cp $(SRCDIR)/libgwm.h $(DESTDIR)/usr/include/libgwm.h
	cp -RT $(SRCDIR)/images $(DESTDIR)/usr/share/images
	cp -RT $(SRCDIR)/fonts $(DESTDIR)/usr/share/fonts

