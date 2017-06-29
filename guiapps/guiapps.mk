SRC := $(shell find $(SRCDIR) -name '*.c')
OUT := $(patsubst $(SRCDIR)/%.c, out/%, $(SRC))
CFLAGS := -Wall -Werror -I$(SRCDIR)/../libddi -I$(SRCDIR)/../libgwm -I$(SRCDIR)/../kernel/include -ggdb
LDFLAGS := -L../libddi -L../libc -L../libgwm -L../fstools -mgui -lcrypt -ggdb

.PHONY: all install
all: $(OUT)

out/%: $(SRCDIR)/%.c
	@mkdir -p out
	$(HOST_GCC) $< -o $@ $(CFLAGS) $(LDFLAGS)

install:
	@mkdir -p $(DESTDIR)/usr/bin
	@mkdir -p $(DESTDIR)/usr/libexec
	@mkdir -p $(DESTDIR)/usr/share/apps
	export DESTDIR=$(DESTDIR) && sh $(SRCDIR)/install.sh $(OUT)
	cp -RT $(SRCDIR)/apps $(DESTDIR)/usr/share/apps
