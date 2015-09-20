.PHONY: all
all: out/cp out/lspci out/clear-screen out/route out/sudo out/mip-install out/passwd out/mount out/gxpad out/mkdir out/sigsegv out/chown out/ls out/sleep out/date out/rmmod out/login out/color out/chgrp out/ping out/ln out/insmod out/whoami out/whois out/env out/halt out/stat out/pwdsetup out/rm out/umount out/crypt out/mkmip out/chmod out/touch out/cat out/netconf
out/cp: src/cp.c
	x86_64-glidix-gcc $< -o $@ 
out/lspci: src/lspci.c
	x86_64-glidix-gcc $< -o $@ 
out/clear-screen: src/clear-screen.c
	x86_64-glidix-gcc $< -o $@ 
out/route: src/route.c
	x86_64-glidix-gcc $< -o $@ 
out/sudo: src/sudo.c
	x86_64-glidix-gcc $< -o $@ -lcrypt
	chmod 6755 $@
out/mip-install: src/mip-install.c
	x86_64-glidix-gcc $< -o $@ 
out/passwd: src/passwd.c
	x86_64-glidix-gcc $< -o $@ -lcrypt
	chmod 6755 $@
out/mount: src/mount.c
	x86_64-glidix-gcc $< -o $@ 
out/gxpad: src/gxpad.c
	x86_64-glidix-gcc $< -o $@ 
out/mkdir: src/mkdir.c
	x86_64-glidix-gcc $< -o $@ 
out/sigsegv: src/sigsegv.c
	x86_64-glidix-gcc $< -o $@ 
out/chown: src/chown.c
	x86_64-glidix-gcc $< -o $@ 
out/ls: src/ls.c
	x86_64-glidix-gcc $< -o $@ 
out/sleep: src/sleep.c
	x86_64-glidix-gcc $< -o $@ 
out/date: src/date.c
	x86_64-glidix-gcc $< -o $@ 
out/rmmod: src/rmmod.c
	x86_64-glidix-gcc $< -o $@ 
out/login: src/login.c
	x86_64-glidix-gcc $< -o $@ -lcrypt
out/color: src/color.c
	x86_64-glidix-gcc $< -o $@ 
out/chgrp: src/chgrp.c
	x86_64-glidix-gcc $< -o $@ 
out/ping: src/ping.c
	x86_64-glidix-gcc $< -o $@ 
out/ln: src/ln.c
	x86_64-glidix-gcc $< -o $@ 
out/insmod: src/insmod.c
	x86_64-glidix-gcc $< -o $@ 
out/whoami: src/whoami.c
	x86_64-glidix-gcc $< -o $@ 
out/whois: src/whois.c
	x86_64-glidix-gcc $< -o $@ 
out/env: src/env.c
	x86_64-glidix-gcc $< -o $@ 
out/halt: src/halt.c
	x86_64-glidix-gcc $< -o $@ 
out/stat: src/stat.c
	x86_64-glidix-gcc $< -o $@ 
out/pwdsetup: src/pwdsetup.c
	x86_64-glidix-gcc $< -o $@ -lcrypt
out/rm: src/rm.c
	x86_64-glidix-gcc $< -o $@ 
out/umount: src/umount.c
	x86_64-glidix-gcc $< -o $@ 
out/crypt: src/crypt.c
	x86_64-glidix-gcc $< -o $@ -lcrypt
out/mkmip: src/mkmip.c
	x86_64-glidix-gcc $< -o $@ 
out/chmod: src/chmod.c
	x86_64-glidix-gcc $< -o $@ 
out/touch: src/touch.c
	x86_64-glidix-gcc $< -o $@ 
out/cat: src/cat.c
	x86_64-glidix-gcc $< -o $@ 
out/netconf: src/netconf.c
	x86_64-glidix-gcc $< -o $@ 
