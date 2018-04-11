SRC := $(shell find $(SRCDIR)/src -name '*.c')
OBJ := $(patsubst $(SRCDIR)/%.c, obj/%.o, $(SRC))
DEP := $(OBJ:.o=.d)
CFLAGS := -Wall -Werror -D_GLIDIX_SOURCE -ggdb

SERVICE_LEVEL ?= 2

.PHONY: install

klogd: $(OBJ)
	$(HOST_GCC) $^ -o $@ -ggdb -ldl

-include $(DEP)

obj/%.d: $(SRCDIR)/%.c
	@mkdir -p $(dir $@)
	$(HOST_GCC) -c $< -MM -MT $(subst .d,.o,$@) -o $@ $(CFLAGS)

obj/%.o: $(SRCDIR)/%.c
	@mkdir -p $(dir $@)
	$(HOST_GCC) -c $< -o $@ $(CFLAGS)

install:
	@mkdir -p $(DESTDIR)/etc/services/$(SERVICE_LEVEL)
	cp klogd $(DESTDIR)/etc/services/$(SERVICE_LEVEL)/klogd.start
	cp $(SRCDIR)/klogd.stop $(DESTDIR)/etc/services/$(SERVICE_LEVEL)/klogd.stop
	chmod -R +x $(DESTDIR)/etc/services/$(SERVICE_LEVEL)

