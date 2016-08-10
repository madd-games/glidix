.PHONY: all
all: out/bin/ifdown out/bin/ifup out/ipv4/static.so out/ipv4/dhcp.so out/ipv4/alic.so out/ipv6/slaac.so out/ipv6/static.so
out/bin/ifdown: src/tools/ifdown.c
	mkdir -p out/bin
	x86_64-glidix-gcc $< -o $@ -Iinclude -ldl -Wl,-export-dynamic
out/bin/ifup: src/tools/ifup.c
	mkdir -p out/bin
	x86_64-glidix-gcc $< -o $@ -Iinclude -ldl -Wl,-export-dynamic
out/ipv4/static.so: src/ipv4/static.c
	mkdir -p out/ipv4
	x86_64-glidix-gcc -fPIC -shared -Iinclude $< -o $@
out/ipv4/dhcp.so: src/ipv4/dhcp.c
	mkdir -p out/ipv4
	x86_64-glidix-gcc -fPIC -shared -Iinclude $< -o $@
out/ipv4/alic.so: src/ipv4/alic.c
	mkdir -p out/ipv4
	x86_64-glidix-gcc -fPIC -shared -Iinclude $< -o $@
out/ipv6/slaac.so: src/ipv6/slaac.c
	mkdir -p out/ipv6
	x86_64-glidix-gcc -fPIC -shared -Iinclude $< -o $@
out/ipv6/static.so: src/ipv6/static.c
	mkdir -p out/ipv6
	x86_64-glidix-gcc -fPIC -shared -Iinclude $< -o $@
