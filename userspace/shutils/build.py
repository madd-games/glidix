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

os.system("mkdir -p out")

utilmap = {}

for name in os.listdir("src"):
	if name.endswith(".c"):
		utilmap["out/"+name[:-2]] = "src/"+name

f = open("build.mk", "wb")
f.write(".PHONY: all\n")
f.write("all: %s\n" % " ".join(utilmap.keys()))

for key, value in utilmap.items():
	f.write("%s: %s\n" % (key, value))
	f.write("\t%s $< -o $@\n" % config["compiler"])

f.close()

os.system("make -f build.mk")
