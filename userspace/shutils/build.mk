.PHONY: all
all: out/cp out/whoami out/whois out/cat out/login out/ls out/crypt out/rm out/mkmip out/mip-install out/stat out/pwdsetup out/mkdir
out/cp: src/cp.c
	x86_64-glidix-gcc $< -o $@ 
out/whoami: src/whoami.c
	x86_64-glidix-gcc $< -o $@ 
out/whois: src/whois.c
	x86_64-glidix-gcc $< -o $@ 
out/cat: src/cat.c
	x86_64-glidix-gcc $< -o $@ 
out/login: src/login.c
	x86_64-glidix-gcc $< -o $@ -lcrypt
out/ls: src/ls.c
	x86_64-glidix-gcc $< -o $@ 
out/crypt: src/crypt.c
	x86_64-glidix-gcc $< -o $@ -lcrypt
out/rm: src/rm.c
	x86_64-glidix-gcc $< -o $@ 
out/mkmip: src/mkmip.c
	x86_64-glidix-gcc $< -o $@ 
out/mip-install: src/mip-install.c
	x86_64-glidix-gcc $< -o $@ 
out/stat: src/stat.c
	x86_64-glidix-gcc $< -o $@ 
out/pwdsetup: src/pwdsetup.c
	x86_64-glidix-gcc $< -o $@ -lcrypt
out/mkdir: src/mkdir.c
	x86_64-glidix-gcc $< -o $@ 
