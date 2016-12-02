SRC := $(shell find $(SRCDIR) -name '*.c')
OUT := $(patsubst $(SRCDIR)/%.c, out/%, $(SRC))
CFLAGS := -Wall -Werror -I$(SRCDIR)/../libddi -I$(SRCDIR)/../libgwm -I$(SRCDIR)/../kernel/include
LDFLAGS := -L../libddi -L../libc -L../libgwm -lgwm -lddi -lcrypt

.PHONY: all install
all: $(OUT)

out/%: $(SRCDIR)/%.c
	@mkdir -p out
	$(HOST_GCC) $< -o $@ $(CFLAGS) $(LDFLAGS)

install:
	@mkdir -p $(DESTDIR)/usr/bin
	@mkdir -p $(DESTDIR)/usr/libexec
	export DESTDIR=$(DESTDIR) && sh $(SRCDIR)/install.sh $(OUT)

