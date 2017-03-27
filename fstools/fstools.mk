C_SRC := $(shell find $(SRCDIR)/lib -name '*.c')
OBJ := $(patsubst $(SRCDIR)/lib/%.c, lib/%.o, $(C_SRC))
DEP := $(OBJ:.o=.d)
CFLAGS := -Wall -Werror -fPIC -I$(SRCDIR)/lib

BIN_SRC := $(shell find $(SRCDIR)/bin -name '*.c')
BIN_OUT := $(patsubst $(SRCDIR)/bin/%.c, bin/%, $(BIN_SRC))

.PHONY: all
all: $(BIN_OUT) libfstools.so

bin/%: $(SRCDIR)/bin/%.c
	@mkdir -p bin
	$(HOST_GCC) $< -o $@ -Wall -Werror -I$(SRCDIR)/lib -Llib -lfstools

libfstools.so: $(OBJ)
	$(HOST_GCC) -shared -o $@ $^

-include $(DEP)

lib/%.d: $(SRCDIR)/lib/%.c
	@mkdir -p lib
	$(HOST_GCC) -c $< -MM -MT $(subst .d,.o,$@) -o $@ $(CFLAGS)

lib/%.o: $(SRCDIR)/lib/%.c
	@mkdir -p lib
	$(HOST_GCC) -c $< -o $@ $(CFLAGS)

.PHONY: install
install:
	@mkdir -p $(DESTDIR)/usr/lib
	@mkdir -p $(DESTDIR)/usr/include
	@mkdir -p $(DESTDIR)/usr/bin
	@mkdir -p $(DESTDIR)/usr/share/mime
	cp libfstools.so $(DESTDIR)/usr/lib/libfstools.so
	cp $(SRCDIR)/lib/fstools.h $(DESTDIR)/usr/include/fstools.h
	cp -RT bin $(DESTDIR)/usr/bin
	cp -RT $(SRCDIR)/mime $(DESTDIR)/usr/share/mime
