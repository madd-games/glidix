#! /usr/bin/python
#	Copyright (c) 2014-2015, Madd Games.
#	All rights reserved.
#	
#	Redistribution and use in source and binary forms, with or without
#	modification, are permitted provided that the following conditions are met:
#	
#	* Redistributions of source code must retain the above copyright notice, this
#	  list of conditions and the following disclaimer.
#	
#	* Redistributions in binary form must reproduce the above copyright notice,
#	  this list of conditions and the following disclaimer in the documentation
#	  and/or other materials provided with the distribution.
#	
#	THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
#	AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
#	IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
#	DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
#	FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
#	DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
#	SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
#	CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
#	OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
#	OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import sys, os
import json

config = {}
f = open("config.json", "rb")
config = json.load(f)
f.close()

os.system("mkdir -p build")
os.system("mkdir -p out")
os.system("mkdir -p initrd")

def doOperation(msg, op):
	sys.stderr.write(msg + " ")
	sys.stderr.flush()
	err = op()
	if err is None:
		sys.stderr.write("OK\n")
	else:
		sys.stderr.write("\n%s\n" % err)
		sys.exit(1)

f = open("build.rule", "rb")
rule = f.read()
f.close()

rules = []
objectFiles = []
depFiles = []
def makeRule(cfile):
	objfile = "build/%s.o" % (cfile.replace("/", "__")[:-2])
	depfile = objfile[:-2] + ".d"
	objectFiles.append(objfile)
	depFiles.append(depfile)

	out = rule
	out = out.replace("%DEPFILE%", depfile)
	out = out.replace("%OBJFILE%", objfile)
	out = out.replace("%CFILE%", cfile)
	out = out.replace("%REPLACE%", cfile.split("/")[-1][:-2]+".o")
	rules.append(out)

for cfile in os.listdir("src"):
	makeRule("src/"+cfile)

for asmfile in os.listdir("asm"):
	if asmfile.endswith(".asm"):
		asmfile = asmfile[:-4]
		rule = "build/asm_%s.o: asm/%s.asm\n" % (asmfile, asmfile)
		rule += "\tnasm -felf64 -o $@ $<\n"
		objectFiles.append("build/asm_%s.o" % asmfile)
		rules.append(rule)

def opCreateBuildMK():
	f = open("build.mk", "wb")
	f.write(".PHONY: all install clean distclean\n")
	f.write("all: out/vmglidix out/vmglidix.tar out/libglidix.a\n")
	f.write("TARGET_CC=%s\n" % config["compiler"])
	f.write("TARGET_AR=%s\n" % config["ar"])
	f.write("TARGET_RANLIB=%s\n" % config["ranlib"])
	f.write("SYSROOT=%s\n" % config["sysroot"])
	f.write("CFLAGS=-ffreestanding -mcmodel=large -mno-red-zone -mno-mmx -mno-sse -mno-sse2 -fno-common -fno-builtin -fno-omit-frame-pointer -I include -Wall -Werror\n")
	f.write("out/vmglidix: linker.ld %s\n" % (" ".join(objectFiles)))
	f.write("\t%s -T linker.ld -o $@ -ffreestanding -O2 -nostdlib %s -lgcc\n" % (config["compiler"], " ".join(objectFiles)))
	f.write("-include %s\n" % (" ".join(depFiles)))
	f.write("initrd/ksyms: elf64.ld %s\n" % (" ".join(objectFiles)))
	f.write("\t%s -T elf64.ld -o build/vmglidix.so -ffreestanding -O2 -nostdlib %s -lgcc\n" % (config["compiler"], " ".join(objectFiles)))
	f.write("\tnm build/vmglidix.so>$@\n")
	f.write("\tobjdump -d build/vmglidix.so>kdebug.txt\n");
	f.write("\n")
	f.write("\n".join(rules))
	f.write("out/vmglidix.tar: initrd/usbs initrd/ksyms\n")
	f.write("\trm -f out/vmglidix.tar\n")
	f.write("\tcd initrd && tar -cf ../out/vmglidix.tar *\n")
	f.write("initrd/usbs: utils/usbs.asm\n")
	f.write("\tnasm $< -o $@ -fbin\n")
	f.write("out/libglidix.a: utils/libglidix/mklibglidix.py\n")
	f.write("\tpython utils/libglidix/mklibglidix.py $(TARGET_CC) out/libglidix.o\n")
	f.write("\t$(TARGET_AR) rvs $@ out/libglidix.o\n")
	f.write("\t$(TARGET_RANLIB) $@\n")
	f.close()

def opCreateISO():
	os.system("mkdir -p isodir/boot")
	os.system("cp out/vmglidix isodir/boot/vmglidix")
	os.system("cp out/vmglidix.tar isodir/boot/vmglidix.tar")
	os.system("mkdir -p isodir/boot/grub")
	os.system("cp grub.cfg isodir/boot/grub/grub.cfg")
	os.system("mkdir -p isodir/initrd")
	os.system("mkdir -p isodir/dev")
	os.system("mkdir -p isodir/mnt")
	os.system("grub-mkrescue -o out/glidix.iso isodir")

doOperation("creating build.mk...", opCreateBuildMK)
sys.stderr.write("running the build...\n")
if os.system("make -f build.mk") != 0:
	sys.exit(1)
opCreateISO()
