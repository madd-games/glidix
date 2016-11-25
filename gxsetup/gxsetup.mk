C_SRC := $(shell find $(SRCDIR) -name '*.c')
OBJ := $(patsubst $(SRCDIR)/%.c, %.o, $(C_SRC))
DEP := $(OBJ:.o=.d)
CFLAGS := -Wall -Werror

gxsetup: $(OBJ)
	@mkdir -p out
	$(HOST_GCC) -o $@ $^ -lcrypt

-include $(DEP)

%.d: $(SRCDIR)/%.c
	$(HOST_GCC) -c $< -MM -MT $(subst .d,.o,$@) -o $@ $(CFLAGS)

%.o: $(SRCDIR)/%.c
	$(HOST_GCC) -c $< -o $@ $(CFLAGS)

.PHONY: install
install:
	@mkdir -p $(DESTDIR)/bin
	@mkdir -p $(DESTDIR)/etc/init
	@mkdir -p $(DESTDIR)/dev
	cp gxsetup $(DESTDIR)/bin/gxsetup
	cp $(SRCDIR)/startup.sh $(DESTDIR)/etc/init/startup.sh
