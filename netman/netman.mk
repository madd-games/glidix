CFLAGS := -I$(SRCDIR)/include -Wall -Werror

TOOLS_SRC := $(shell find $(SRCDIR)/tools -name '*.c')
TOOLS_OUT := $(patsubst $(SRCDIR)/tools/%.c, tools/%, $(TOOLS_SRC))
TOOLS_LDFLAGS := -ldl -Wl,-export-dynamic

IPV4_SRC := $(shell find $(SRCDIR)/ipv4 -name '*.c')
IPV4_OUT := $(patsubst $(SRCDIR)/ipv4/%.c, ipv4/%.so, $(IPV4_SRC))

IPV6_SRC := $(shell find $(SRCDIR)/ipv6 -name '*.c')
IPV6_OUT := $(patsubst $(SRCDIR)/ipv6/%.c, ipv6/%.so, $(IPV6_SRC))

.PHONY: all install
all: $(TOOLS_OUT) $(IPV4_OUT) $(IPV6_OUT)

tools/%: $(SRCDIR)/tools/%.c $(SRCDIR)/include/gxnetman.h
	@mkdir -p tools
	$(HOST_GCC) $< -o $@ $(CFLAGS) $(TOOLS_LDFLAGS)

ipv4/%.so: $(SRCDIR)/ipv4/%.c
	@mkdir -p ipv4
	$(HOST_GCC) -fPIC -shared $< -o $@ $(CFLAGS)

ipv6/%.so: $(SRCDIR)/ipv6/%.c
	@mkdir -p ipv6
	$(HOST_GCC) -fPIC -shared $< -o $@ $(CFLAGS)

install:
	@mkdir -p $(DESTDIR)/usr/bin
	@mkdir -p $(DESTDIR)/usr/include
	@mkdir -p $(DESTDIR)/usr/libexec/netman/ipv4
	@mkdir -p $(DESTDIR)/usr/libexec/netman/ipv6
	cp -RT $(SRCDIR)/include $(DESTDIR)/usr/include
	cp -RT tools $(DESTDIR)/usr/bin
	cp -RT ipv4 $(DESTDIR)/usr/libexec/netman/ipv4
	cp -RT ipv6 $(DESTDIR)/usr/libexec/netman/ipv6
