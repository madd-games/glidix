C_SRC := $(shell find $(SRCDIR) -name '*.c')
OBJ := $(patsubst $(SRCDIR)/%.c, %.o, $(C_SRC))
DEP := $(OBJ:.o=.d)
CFLAGS := -Wall -Werror -fPIC

libddi.so: $(OBJ)
	@mkdir -p out
	$(HOST_GCC) -shared -o $@ $^

-include $(DEP)

%.d: $(SRCDIR)/%.c
	$(HOST_GCC) -c $< -MM -MT $(subst .d,.o,$@) -o $@ $(CFLAGS)

%.o: $(SRCDIR)/%.c
	$(HOST_GCC) -c $< -o $@ $(CFLAGS)

.PHONY: install
install:
	@mkdir -p $(DESTDIR)/usr/lib
	@mkdir -p $(DESTDIR)/usr/include
	cp libddi.so $(DESTDIR)/usr/lib/libddi.so
	cp libddi.h $(DESTDIR)/usr/include/libddi.h
