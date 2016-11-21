/*
	Glidix kernel

	Copyright (c) 2014-2016, Madd Games.
	All rights reserved.
	
	Redistribution and use in source and binary forms, with or without
	modification, are permitted provided that the following conditions are met:
	
	* Redistributions of source code must retain the above copyright notice, this
	  list of conditions and the following disclaimer.
	
	* Redistributions in binary form must reproduce the above copyright notice,
	  this list of conditions and the following disclaimer in the documentation
	  and/or other materials provided with the distribution.
	
	THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
	AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
	IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
	DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
	FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
	DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
	SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
	CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
	OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
	OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <glidix/symtab.h>
#include <glidix/vfs.h>
#include <glidix/memory.h>
#include <glidix/string.h>
#include <glidix/console.h>
#include <glidix/elf64.h>
#include <glidix/common.h>

static Elf64_Sym *symbols;
static const char *symStrings;
extern uint8_t initrdImage[];
static size_t symbolCount;

void initSymtab()
{
	symbolCount = bootInfo->numSymbols;
	symbols = (Elf64_Sym*) ((uint64_t)initrdImage + bootInfo->initrdSymtabOffset);
	symStrings = (const char*) ((uint64_t)initrdImage + bootInfo->initrdStrtabOffset);
};

void *getSymbol(const char *name)
{
	size_t i;
	for (i=0; i<symbolCount; i++)
	{
		if (strcmp(&symStrings[symbols[i].st_name], name) == 0)
		{
			return (void*) symbols[i].st_value;
		};
	};
	
	return NULL;
};

void findSymbolForAddr(uint64_t addr, Symbol *symbol)
{
	size_t i;
	size_t bestMatch = 0;
	for (i=0; i<symbolCount; i++)
	{
		if ((symbols[i].st_value <= addr) && (symbols[i].st_value > symbols[bestMatch].st_value))
		{
			bestMatch = i;
		};
	};
	
	symbol->name = &symStrings[symbols[bestMatch].st_name];
	symbol->ptr = (void*) symbols[bestMatch].st_value;
};
