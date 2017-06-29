C_SRC := $(shell find $(SRCDIR) -name '*.c')
OBJ := $(patsubst $(SRCDIR)/%.c, %.o, $(C_SRC))
DEP := $(OBJ:.o=.d)
CFLAGS := -Wall -Werror -L../libc -ggdb

sh: $(OBJ)
	@mkdir -p out
	$(HOST_GCC) -o $@ $^ -ggdb

-include $(DEP)

%.d: $(SRCDIR)/%.c
	$(HOST_GCC) -c $< -MM -MT $(subst .d,.o,$@) -o $@ $(CFLAGS)

%.o: $(SRCDIR)/%.c
	$(HOST_GCC) -c $< -o $@ $(CFLAGS)

.PHONY: install
install:
	@mkdir -p $(DESTDIR)/bin
	cp sh $(DESTDIR)/bin/sh
	cp sh $(DESTDIR)/bin/glx-sh
