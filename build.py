#! /usr/bin/python
#	Copyright (c) 2014, Madd Games.
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

def opCreateBuildMK():
	f = open("build.mk", "wb")
	f.write("CFLAGS=-ffreestanding -mcmodel=large -mno-red-zone -mno-mmx -mno-sse -mno-sse2 -fno-common\n")
	f.write("out/vmglidix: build/bootstrap.o %s\n" % (" ".join(objectFiles)))
	f.write("\tx86_64-elf-gcc -T linker.ld -o $@ -ffreestanding -O2 -nostdlib $^ -lgcc\n")
	f.write("-include %s\n" % (" ".join(depFiles)))
	f.write("\n")
	f.write("build/bootstrap.o: bootstrap/bootstrap.asm\n")
	f.write("\tnasm bootstrap/bootstrap.asm -o build/bootstrap.o -felf64\n")
	f.write("\n".join(rules))
	f.close()

def opCreateISO():
	os.system("mkdir -p isodir/boot")
	os.system("cp out/vmglidix isodir/boot/vmglidix")
	os.system("mkdir -p isodir/boot/grub")
	os.system("cp grub.cfg isodir/boot/grub/grub.cfg")
	os.system("grub-mkrescue -o out/glidix.iso isodir")

doOperation("creating build.mk...", opCreateBuildMK)
sys.stderr.write("running the build...\n")
os.system("make -f build.mk")
opCreateISO()
