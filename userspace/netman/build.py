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

os.system("mkdir -p out")

utilmap = {}
map4 = {}
map6 = {}
outputs = []

for name in os.listdir("src/tools"):
	if name.endswith(".c"):
		utilmap["out/bin/"+name[:-2]] = "src/tools/"+name

for name in os.listdir("src/ipv4"):
	if name.endswith(".c"):
		map4["out/ipv4/"+name[:-2]+".so"] = "src/ipv4/"+name

for name in os.listdir("src/ipv6"):
	if name.endswith(".c"):
		map6["out/ipv6/"+name[:-2]+".so"] = "src/ipv6/"+name
		
outputs.extend(utilmap.keys())
outputs.extend(map4.keys())
outputs.extend(map6.keys())

f = open("build.mk", "wb")
f.write(".PHONY: all\n")
f.write("all: %s\n" % " ".join(outputs))

for key, value in utilmap.items():
	f.write("%s: %s\n" % (key, value))
	extra = "-Iinclude -ldl -Wl,-export-dynamic"
	f.write("\tmkdir -p out/bin\n")
	f.write("\t%s $< -o $@ %s\n" % (config["compiler"], extra))

for key, value in map4.items():
	f.write("%s: %s\n" % (key, value))
	f.write("\tmkdir -p out/ipv4\n")
	f.write("\t%s -fPIC -shared -Iinclude $< -o $@\n" % config["compiler"])

for key, value in map6.items():
	f.write("%s: %s\n" % (key, value))
	f.write("\tmkdir -p out/ipv6\n")
	f.write("\t%s -fPIC -shared -Iinclude $< -o $@\n" % config["compiler"])
	
f.close()

sys.exit(os.system("make -f build.mk"))
