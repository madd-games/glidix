C_SRC := $(shell find $(SRCDIR) -name '*.c')
OBJ := $(patsubst $(SRCDIR)/%.c, %.o, $(C_SRC))
DEP := $(OBJ:.o=.d)
CFLAGS := -Wall -Werror -fPIC -I$(SYSROOT)/usr/include/freetype2 -I$(SRCDIR)/include -I$(SRCDIR)/../libddi -O3 -ggdb

libGL.so: $(OBJ)
	@mkdir -p out
	$(HOST_GCC) -shared -o $@ $^ -L ../libddi -lddi -O3 -ggdb

-include $(DEP)

%.d: $(SRCDIR)/%.c
	$(HOST_GCC) -c $< -MM -MT $(subst .d,.o,$@) -o $@ $(CFLAGS)

%.o: $(SRCDIR)/%.c
	$(HOST_GCC) -c $< -o $@ $(CFLAGS)

.PHONY: install
install:
	@mkdir -p $(DESTDIR)/usr/lib
	@mkdir -p $(DESTDIR)/usr/include
	cp libGL.so $(DESTDIR)/usr/lib/libGL.so
	cp -RT $(SRCDIR)/include $(DESTDIR)/usr/include
