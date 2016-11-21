init: $(SRCDIR)/init.c
	$(HOST_GCC) $< -o $@ -Wall -Werror -L../libc

