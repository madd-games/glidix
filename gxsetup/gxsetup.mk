C_SRC := $(shell find $(SRCDIR) -name '*.c')
OBJ := $(patsubst $(SRCDIR)/%.c, %.o, $(C_SRC))
DEP := $(OBJ:.o=.d)
CFLAGS := -Wall -Werror -ggdb

gxsetup: $(OBJ)
	@mkdir -p out
	$(HOST_GCC) -o $@ $^ -lcrypt -ggdb

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
	@mkdir -p $(DESTDIR)/proc
	@mkdir -p $(DESTDIR)/mnt
	@mkdir -p $(DESTDIR)/sys/mod
	@mkdir -p $(DESTDIR)/initrd
	cp gxsetup $(DESTDIR)/bin/gxsetup
	cp $(SRCDIR)/startup.sh $(DESTDIR)/etc/init/startup.sh
	cp $(SRCDIR)/passwd $(DESTDIR)/etc/passwd
	cp $(SRCDIR)/shadow $(DESTDIR)/etc/shadow
	cp $(SRCDIR)/group $(DESTDIR)/etc/group
	cp $(SRCDIR)/sudoers $(DESTDIR)/etc/sudoers
