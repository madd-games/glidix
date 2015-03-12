.PHONY: all
all: out/cp out/stat out/ls out/mkdir
out/cp: src/cp.c
	x86_64-glidix-gcc $< -o $@
out/stat: src/stat.c
	x86_64-glidix-gcc $< -o $@
out/ls: src/ls.c
	x86_64-glidix-gcc $< -o $@
out/mkdir: src/mkdir.c
	x86_64-glidix-gcc $< -o $@
