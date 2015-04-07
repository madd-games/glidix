.PHONY: all
all: out/cp out/cat out/whoami out/whois out/chgrp out/sudo out/env out/rmmod out/login out/ls out/crypt out/rm out/mkmip out/chmod out/touch out/mip-install out/passwd out/stat out/pwdsetup out/mkdir out/chown
out/cp: src/cp.c
	x86_64-glidix-gcc $< -o $@ 
out/cat: src/cat.c
	x86_64-glidix-gcc $< -o $@ 
out/whoami: src/whoami.c
	x86_64-glidix-gcc $< -o $@ 
out/whois: src/whois.c
	x86_64-glidix-gcc $< -o $@ 
out/chgrp: src/chgrp.c
	x86_64-glidix-gcc $< -o $@ 
out/sudo: src/sudo.c
	x86_64-glidix-gcc $< -o $@ -lcrypt
	chmod 6755 $@
out/env: src/env.c
	x86_64-glidix-gcc $< -o $@ 
out/rmmod: src/rmmod.c
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
out/chmod: src/chmod.c
	x86_64-glidix-gcc $< -o $@ 
out/touch: src/touch.c
	x86_64-glidix-gcc $< -o $@ 
out/mip-install: src/mip-install.c
	x86_64-glidix-gcc $< -o $@ 
out/passwd: src/passwd.c
	x86_64-glidix-gcc $< -o $@ -lcrypt
	chmod 6755 $@
out/stat: src/stat.c
	x86_64-glidix-gcc $< -o $@ 
out/pwdsetup: src/pwdsetup.c
	x86_64-glidix-gcc $< -o $@ -lcrypt
out/mkdir: src/mkdir.c
	x86_64-glidix-gcc $< -o $@ 
out/chown: src/chown.c
	x86_64-glidix-gcc $< -o $@ 
