.PHONY: all
all: out/sh
TARGET_CC=x86_64-glidix-gcc
TARGET_AR=x86_64-glidix-ar
TARGET_RANLIB=x86_64-glidix-ranlib
PREFIX=/glidix
CFLAGS=-mno-mmx -mno-sse -mno-sse2 -I include -Wall -Werror
out/sh: build/src__sh.o build/src__echo.o build/src__command.o build/src__exit.o build/src__cd.o
	x86_64-glidix-gcc -o $@ $^
-include build/src__sh.d build/src__echo.d build/src__command.d build/src__exit.d build/src__cd.d
build/src__sh.d: src/sh.c
	set -e; rm -f $@; \
	$(TARGET_CC) -M $(CFLAGS) $< > $@.$$$$; \
	sed 's,sh.o[ :]*,build/src__sh.o $@ : ,g' < $@.$$$$ > $@; \
	rm -f $@.$$$$

build/src__sh.o: src/sh.c
	$(TARGET_CC) -c $< -o $@ $(CFLAGS)

build/src__echo.d: src/echo.c
	set -e; rm -f $@; \
	$(TARGET_CC) -M $(CFLAGS) $< > $@.$$$$; \
	sed 's,echo.o[ :]*,build/src__echo.o $@ : ,g' < $@.$$$$ > $@; \
	rm -f $@.$$$$

build/src__echo.o: src/echo.c
	$(TARGET_CC) -c $< -o $@ $(CFLAGS)

build/src__command.d: src/command.c
	set -e; rm -f $@; \
	$(TARGET_CC) -M $(CFLAGS) $< > $@.$$$$; \
	sed 's,command.o[ :]*,build/src__command.o $@ : ,g' < $@.$$$$ > $@; \
	rm -f $@.$$$$

build/src__command.o: src/command.c
	$(TARGET_CC) -c $< -o $@ $(CFLAGS)

build/src__exit.d: src/exit.c
	set -e; rm -f $@; \
	$(TARGET_CC) -M $(CFLAGS) $< > $@.$$$$; \
	sed 's,exit.o[ :]*,build/src__exit.o $@ : ,g' < $@.$$$$ > $@; \
	rm -f $@.$$$$

build/src__exit.o: src/exit.c
	$(TARGET_CC) -c $< -o $@ $(CFLAGS)

build/src__cd.d: src/cd.c
	set -e; rm -f $@; \
	$(TARGET_CC) -M $(CFLAGS) $< > $@.$$$$; \
	sed 's,cd.o[ :]*,build/src__cd.o $@ : ,g' < $@.$$$$ > $@; \
	rm -f $@.$$$$

build/src__cd.o: src/cd.c
	$(TARGET_CC) -c $< -o $@ $(CFLAGS)
