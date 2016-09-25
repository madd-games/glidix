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

static Symbol *symtab = NULL;
int _symtab_test;

static void *parseAddr(char *scan)
{
	uint64_t addr = 0;
	int count = 16;
	while (count--)
	{
		char c = *scan++;
		uint64_t digit;

		if (c >= 'a')
		{
			digit = c - 'a' + 10;
		}
		else
		{
			digit = c - '0';
		};

		addr = addr * 16 + digit;
	};

	return (void*) addr;
};

void initSymtab()
{
	// initialize the symtab with just an empty entry.
	Symbol *last = (Symbol*) kmalloc(sizeof(Symbol));
	strcpy(last->name, "__glidix_nullsym__");
	last->ptr = NULL;
	last->next = NULL;
	symtab = last;

	kprintf("Loading kernel symbol table... ");
	int error;
	File *file = vfsOpen("/initrd/ksyms", 0, &error);
	if (file == NULL)
	{
		FAILED();
		panic("could not load /initrd/ksyms!");
	};

	char addrhex[16];
	char dump[3];
	while (1)
	{
		if (vfsRead(file, addrhex, 16) != 16)
		{
			break;
		};

		if (vfsRead(file, dump, 3) != 3)
		{
			FAILED();
			panic("could not parse /initrd/ksyms!");
		};

		Symbol *sym = (Symbol*) kmalloc(sizeof(Symbol));
		char c;
		char *p = sym->name;
		while (1)
		{
			if (vfsRead(file, &c, 1) != 1)
			{
				FAILED();
				panic("could not parse /initrd/ksyms!");
			};

			if (c == '\n')
			{
				break;
			};

			*p++ = c;
		};
		*p = 0;

		sym->ptr = parseAddr(addrhex);
		sym->next = NULL;
		last->next = sym;
		last = sym;
	};

	DONE();

	kprintf("Validating the symbol table... ");
	void *sym = getSymbol("_symtab_test");
	if (sym == NULL)
	{
		FAILED();
		panic("could not resolve _symtab_test dynamically");
	};

	if (sym != &_symtab_test)
	{
		FAILED();
		panic("the resolved address of _symtab_test is invalid: should be %a, is %a", &_symtab_test, sym);
	};

	DONE();
};

void *getSymbol(const char *name)
{
	Symbol *sym = symtab;
	while (strcmp(sym->name, name) != 0)
	{
		sym = sym->next;
		if (sym == NULL)
		{
			return NULL;
		};
	};

	return sym->ptr;
};

Symbol *findSymbolForAddr(uint64_t addr)
{
	Symbol *sym = symtab;
	Symbol *possible = symtab;

	while (sym != NULL)
	{
		uint64_t saddr = (uint64_t) sym->ptr;
		uint64_t top = (uint64_t) possible->ptr;

		if ((saddr > top) && (saddr <= addr))
		{
			possible = sym;
		};

		sym = sym->next;
	};

	return possible;
};

