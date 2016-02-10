#! /usr/bin/python
#	Copyright (c) 2014-2016, Madd Games.
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
os.system("mkdir -p out/lib")

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

cfiles = []
def listdir(dirname):
	for name in os.listdir(dirname):
		path = dirname + "/" + name
		if os.path.isdir(path):
			listdir(path)
		elif path.endswith(".c"):
			cfiles.append(path)

listdir("src")
for cfile in cfiles:
	makeRule(cfile)

for asmfile in os.listdir("asm"):
	if asmfile.endswith(".s"):
		asmfile = asmfile[:-2]
		rule = "build/asm_%s.o: asm/%s.s\n" % (asmfile, asmfile)
		rule += "\t%s -c $< -o $@\n" % config["assembler"]
		objectFiles.append("build/asm_%s.o" % asmfile)
		rules.append(rule)

def opCreateBuildMK():
	f = open("build.mk", "wb")
	f.write(".PHONY: all\n")
	f.write("all: out/lib/libc.so out/lib/libdl.so out/lib/libm.so out/lib/libcrypt.so out/lib/libc.a out/lib/crt0.o out/lib/crti.o out/lib/crtn.o\n")
	f.write("TARGET_CC=%s\n" % config["compiler"])
	f.write("TARGET_AR=%s\n" % config["ar"])
	f.write("TARGET_RANLIB=%s\n" % config["ranlib"])
	f.write("PREFIX=%s\n" % config["prefix"])
	f.write("CFLAGS=-mno-mmx -I include -Wall -Werror -fPIC\n")
	f.write("out/lib/libc.so: libglidix.o %s\n" % (" ".join(objectFiles)))
	f.write("\t$(TARGET_CC) -shared -o $@ $^\n");
	f.write("out/lib/libdl.so: support/libdl.c\n")
	f.write("\t$(TARGET_CC) -shared -o $@ $^ -fPIC\n")
	f.write("out/lib/libm.so: support/libm.c\n")
	f.write("\t$(TARGET_CC) -shared -o $@ $^ -fPIC\n")
	f.write("out/lib/libcrypt.so: support/libcrypt.c\n")
	f.write("\t$(TARGET_CC) -shared -o $@ $^ -fPIC\n")
	f.write("libglidix.o: %s/lib/libglidix.a\n" % config["prefix"])
	f.write("\t%s x %s/lib/libglidix.a\n" % (config["ar"], config["prefix"]))
	f.write("out/lib/libc.a: libglidix.o %s\n" % (" ".join(objectFiles)))
	f.write("\t%s rvs $@ $^\n" % config["ar"])
	f.write("\t%s $@\n" % config["ranlib"])
	f.write("-include %s\n" % (" ".join(depFiles)))
	f.write("out/lib/crt0.o: crt/crt0.asm\n")
	f.write("\tnasm -felf64 -o $@ $<\n")
	f.write("out/lib/crti.o: crt/crti.asm\n")
	f.write("\tnasm -felf64 -o $@ $<\n")
	f.write("out/lib/crtn.o: crt/crtn.asm\n")
	f.write("\tnasm -felf64 -o $@ $<\n")
	f.write("\n")
	f.write("\n".join(rules))
	f.close()

doOperation("creating build.mk...", opCreateBuildMK)
sys.stderr.write("running the build...\n")
if os.system("make -f build.mk") != 0:
	sys.exit(1)
