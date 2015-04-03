.PHONY: all
all: out/cp out/whoami out/whois out/login out/ls out/crypt out/cat out/stat out/pwdsetup out/mkdir
out/cp: src/cp.c
	x86_64-glidix-gcc $< -o $@ 
out/whoami: src/whoami.c
	x86_64-glidix-gcc $< -o $@ 
out/whois: src/whois.c
	x86_64-glidix-gcc $< -o $@ 
out/login: src/login.c
	x86_64-glidix-gcc $< -o $@ -lcrypt
out/ls: src/ls.c
	x86_64-glidix-gcc $< -o $@ 
out/crypt: src/crypt.c
	x86_64-glidix-gcc $< -o $@ -lcrypt
out/cat: src/cat.c
	x86_64-glidix-gcc $< -o $@ 
out/stat: src/stat.c
	x86_64-glidix-gcc $< -o $@ 
out/pwdsetup: src/pwdsetup.c
	x86_64-glidix-gcc $< -o $@ -lcrypt
out/mkdir: src/mkdir.c
	x86_64-glidix-gcc $< -o $@ 
