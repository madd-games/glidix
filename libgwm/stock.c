/*
	Glidix GUI

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

#include <libgwm.h>

typedef struct
{
	int symbol;
	const char *label;
} StockLabel;

typedef struct
{
	int symbol;
	const char *themeProp;
	DDISurface *surf;
} StockIcon;

static StockLabel labels[] = {
	{GWM_SYM_OK,			"OK"},
	{GWM_SYM_CANCEL,		"Cancel"},
	{GWM_SYM_YES,			"Yes"},
	{GWM_SYM_NO,			"No"},
	{GWM_SYM_EXIT,			"Exit"},
	{GWM_SYM_BACK,			"Back"},
	{GWM_SYM_FORWARD,		"Forward"},
	{GWM_SYM_UP,			"Up"},
	{GWM_SYM_DOWN,			"Down"},
	{GWM_SYM_NEW_FILE,		"New..."},
	{GWM_SYM_OPEN_FILE,		"Open..."},
	{GWM_SYM_SAVE,			"Save"},
	{GWM_SYM_SAVE_AS,		"Save as..."},
	{GWM_SYM_SAVE_COPY,		"Save copy"},
	{GWM_SYM_REFRESH,		"Refresh"},
	{GWM_SYM_CONFIRM,		"Confirm"},
	{GWM_SYM_CUT,			"Cut"},
	{GWM_SYM_COPY,			"Copy"},
	{GWM_SYM_PASTE,			"Paste"},
	{GWM_SYM_ADD,			"Add"},
	{GWM_SYM_REMOVE,		"Remove"},
	{GWM_SYM_HOME,			"Home"},
	{GWM_SYM_UNDO,			"Redo"},
	{GWM_SYM_ADJ_LEFT,		"Adjust left"},
	{GWM_SYM_ADJ_CENTER,		"Adjust to center"},
	{GWM_SYM_ADJ_RIGHT,		"Adjust right"},
	{GWM_SYM_JUSTIFY,		"Justify"},
	{GWM_SYM_SELECT_ALL,		"Select all"},
	{GWM_SYM_EDIT,			"Edit"},
	{GWM_SYM_BOLD,			"Bold"},
	{GWM_SYM_ITALIC,		"Italic"},
	{GWM_SYM_UNDERLINE,		"Underline"},
	{GWM_SYM_STRIKE,		"Strike"},
	{GWM_SYM_RENAME,		"Rename"},
	
	// LIST TERMINATOR
	{GWM_SYM_NONE, ""},
};

static StockIcon icons[] = {
	{GWM_SYM_BACK,			"gwm.toolkit.stock.back",		NULL},
	{GWM_SYM_FORWARD,		"gwm.toolkit.stock.forward",		NULL},
	{GWM_SYM_UP,			"gwm.toolkit.stock.up",			NULL},
	{GWM_SYM_DOWN,			"gwm.toolkit.stock.down",		NULL},
	{GWM_SYM_NEW_FILE,		"gwm.toolkit.stock.newfile",		NULL},
	{GWM_SYM_OPEN_FILE,		"gwm.toolkit.stock.openfile",		NULL},
	{GWM_SYM_SAVE,			"gwm.toolkit.stock.save",		NULL},
	{GWM_SYM_SAVE_AS,		"gwm.toolkit.stock.saveas",		NULL},
	{GWM_SYM_SAVE_COPY,		"gwm.toolkit.stock.savecopy",		NULL},
	{GWM_SYM_REFRESH,		"gwm.toolkit.stock.refresh",		NULL},
	{GWM_SYM_CANCEL,		"gwm.toolkit.stock.cancel",		NULL},
	{GWM_SYM_CONFIRM,		"gwm.toolkit.stock.confirm",		NULL},
	{GWM_SYM_CUT,			"gwm.toolkit.stock.cut",		NULL},
	{GWM_SYM_COPY,			"gwm.toolkit.stock.copy",		NULL},
	{GWM_SYM_PASTE,			"gwm.toolkit.stock.paste",		NULL},
	{GWM_SYM_ADD,			"gwm.toolkit.stock.add",		NULL},
	{GWM_SYM_REMOVE,		"gwm.toolkit.stock.remove",		NULL},
	{GWM_SYM_HOME,			"gwm.toolkit.stock.home",		NULL},
	{GWM_SYM_UNDO,			"gwm.toolkit.stock.undo",		NULL},
	{GWM_SYM_REDO,			"gwm.toolkit.stock.redo",		NULL},
	{GWM_SYM_ADJ_LEFT,		"gwm.toolkit.stock.adjleft",		NULL},
	{GWM_SYM_ADJ_CENTER,		"gwm.toolkit.stock.adjcenter",		NULL},
	{GWM_SYM_ADJ_RIGHT,		"gwm.toolkit.stock.adjright",		NULL},
	{GWM_SYM_JUSTIFY,		"gwm.toolkit.stock.justify",		NULL},
	{GWM_SYM_SELECT_ALL,		"gwm.toolkit.stock.selectall",		NULL},
	{GWM_SYM_EDIT,			"gwm.toolkit.stock.edit",		NULL},
	{GWM_SYM_BOLD,			"gwm.toolkit.stock.bold",		NULL},
	{GWM_SYM_ITALIC,		"gwm.toolkit.stock.italic",		NULL},
	{GWM_SYM_UNDERLINE,		"gwm.toolkit.stock.underline",		NULL},
	{GWM_SYM_STRIKE,		"gwm.toolkit.stock.strike",		NULL},
	
	// LIST TERMINATOR
	{GWM_SYM_NONE, "", NULL}
};

const char *gwmGetStockLabel(int symbol)
{
	StockLabel *scan;
	for (scan=labels; scan->symbol!=GWM_SYM_NONE; scan++)
	{
		if (scan->symbol == symbol) return scan->label;
	};
	
	return "??";
};

DDISurface* gwmGetStockIcon(int symbol)
{
	StockIcon *scan;
	for (scan=icons; scan->symbol!=GWM_SYM_NONE; scan++)
	{
		if (scan->symbol == symbol)
		{
			if (scan->surf == NULL)
			{
				scan->surf = (DDISurface*) gwmGetThemeProp(scan->themeProp, GWM_TYPE_SURFACE, NULL);
			};
			
			return scan->surf;
		};
	};
	
	return NULL;
};
