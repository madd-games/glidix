.PHONY: all
all: out/stat out/ls out/mkdir
out/stat: src/stat.c
	x86_64-glidix-gcc $< -o $@
out/ls: src/ls.c
	x86_64-glidix-gcc $< -o $@
out/mkdir: src/mkdir.c
	x86_64-glidix-gcc $< -o $@
