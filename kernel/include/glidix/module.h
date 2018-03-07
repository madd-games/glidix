/*
	Glidix kernel

	Copyright (c) 2014-2017, Madd Games.
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

#ifndef __glidix_module_h
#define __glidix_module_h

/**
 * Linking and unloading modules.
 */

#include <glidix/common.h>
#include <glidix/elf64.h>
#include <glidix/vfs.h>

/**
 * Flags for _glidix_insmod().
 */
#define	INSMOD_VERBOSE			(1 << 0)

/**
 * Flags for _glidix_rmmod().
 */
#define	RMMOD_VERBOSE			(1 << 0)
#define	RMMOD_FORCE			(1 << 1)

/**
 * Possible return values from MODULE_INIT.
 */
#define	MODINIT_OK			0			/* initialization successful and module loaded */
#define	MODINIT_FATAL			1			/* fatal error occured and module force-unloaded */
#define	MODINIT_CANCEL			2			/* module decided it's not needed, so it was safely unloaded */

/**
 * Those macros are to be used by modules.
 */
#define	MODULE_INIT(...)		int __module_init(__VA_ARGS__)
#define	MODULE_FINI(...)		int __module_fini(__VA_ARGS__)

/**
 * This macro evaluates to a pointer to the current module, when used inside module code. It returns a
 * pointer to the external variable "__this_module" declared below. That variable does not actually exist
 * but the module loader resolves the symbol to the module structure's address.
 */
#define	THIS_MODULE			(&__this_module)

typedef struct _Module
{
	char				name[128];
	int				block;
	int				numSectors;		// number of 2MB blocks assigned to this module
	uint64_t*			frames;			// allocated frames - number of them = numSectors+numSectors*512
	uint64_t			baseAddr;
	Elf64_Sym*			symtab;
	uint64_t			numSymbols;
	const char*			strings;
	struct _Module*			next;
	void (*fini)(void);
	int (*modfini)(void);
} Module;

/**
 * Information about a module as presented by sys_modstat().
 */
typedef struct
{
	char				mod_name[128];
	int				mod_block;
	char				mod_resv[64];
} ModuleState;

typedef struct
{
	const char*			modname;
	const char*			symname;
	uint64_t			offset;
} SymbolInfo;

void initModuleInterface();
int insmod(const char *modname, const char *path, const char *opt, int flags);
int rmmod(const char *modname, int flags);
void dumpModules();
void findDebugSymbolInModules(uint64_t addr, SymbolInfo *symInfo);
void rmmodAll();

/**
 * Get information about a module in block 'block'. If successful, 0 is returned. If the module does
 * not exist, ENOENT is returned. Other errors could also be possible in the future - the error number
 * is returned on error.
 */
int modStat(int block, ModuleState *state);

/**
 * See above (THIS_MODULE).
 */
extern Module __this_module;

#endif
