SRC := $(shell find $(SRCDIR) -name '*.c')
OUT := $(patsubst $(SRCDIR)/%.c, out/%, $(SRC))
CFLAGS := -Wall -Werror
LDFLAGS := -lcrypt -L../libc

.PHONY: all install
all: $(OUT)

out/%: $(SRCDIR)/%.c
	@mkdir -p out
	$(HOST_GCC) $< -o $@ $(CFLAGS) $(LDFLAGS)

install:
	@mkdir -p $(DESTDIR)/usr/bin
	@mkdir -p $(DESTDIR)/usr/libexec
	@mkdir -p $(DESTDIR)/etc
	export DESTDIR=$(DESTDIR) && sh $(SRCDIR)/install.sh $(OUT)
	cp $(SRCDIR)/shutils-setup.sh $(DESTDIR)/usr/libexec/shutils-setup.sh
	cp $(SRCDIR)/dispdevs.dat $(DESTDIR)/etc/dispdevs.dat
