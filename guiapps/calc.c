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

#include <sys/glidix.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libddi.h>
#include <pthread.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <libgwm.h>
#include <assert.h>
#include <ctype.h>

enum
{
	SYM_CALC = GWM_SYM_USER,
	SYM_CALC_RUN = GWM_SYM_USER + 14,
};

enum
{
	TOK_NUM,
	TOK_ADD,
	TOK_SUB,
	TOK_MULT,
	TOK_DIV,
	TOK_OPEN_BRACKET,
	TOK_CLOSE_BRACKET
};

typedef struct CalcToken_
{
	int				type;
	double				value;
	struct CalcToken_*		next;
} CalcToken;

GWMTextField *txtCalc;

static const char* pad[] = {
	"7", "8", "9", "+",
	"4", "5", "6", "-",
	"1", "2", "3", "×",
	"0", ".", "=", "÷"
};

static void pushToken(CalcToken **first, CalcToken **last, int type, double value)
{	
	CalcToken *tok = (CalcToken*) malloc(sizeof(CalcToken));
	tok->type = type;
	tok->value = value;
	tok->next = NULL;
	
	if ((*first) == NULL)
	{
		*first = *last = tok;
	}
	else
	{
		(*last)->next = tok;
		*last = tok;
	};
};

static double evalExpr(CalcToken *toklist, CalcToken **endptr);

static double evalPrimary(CalcToken *toklist, CalcToken **endptr)
{
	if (toklist == NULL)
	{
		*endptr = toklist;
		return 0.0;
	};
	
	if (toklist->type == TOK_NUM)
	{
		*endptr = toklist->next;
		return toklist->value;
	}
	else if (toklist->type == TOK_OPEN_BRACKET)
	{
		CalcToken *temp;
		double x = evalExpr(toklist->next, &temp);
		if (temp == toklist->next)
		{
			*endptr = toklist;
			return 0.0;
		};
		
		if (temp == NULL)
		{
			*endptr = NULL;
			return x;
		};
		
		if (temp->type != TOK_CLOSE_BRACKET)
		{
			*endptr = toklist;
			return 0.0;
		};
		
		*endptr = temp->next;
		return x;
	}
	else
	{
		*endptr = toklist;
		return 0.0;
	};
};

static double evalProduct(CalcToken *toklist, CalcToken **endptr)
{
	if (toklist == NULL)
	{
		*endptr = toklist;
		return 0.0;
	};
	
	CalcToken *temp;
	double x = evalPrimary(toklist, &temp);
	
	if (temp == toklist)
	{
		*endptr = toklist;
		return 0.0;
	};
	
	toklist = temp;
	while (toklist != NULL && (toklist->type == TOK_MULT || toklist->type == TOK_DIV))
	{
		int op = toklist->type;
		toklist = toklist->next;
		
		double y = evalPrimary(toklist, &temp);
		if (temp == toklist)
		{
			*endptr = NULL;
			return 0.0;
		};
		
		toklist = temp;
		if (op == TOK_MULT)
		{
			x *= y;
		}
		else
		{
			x /= y;
		};
	};
	
	*endptr = toklist;
	return x;
};

static double evalSum(CalcToken *toklist, CalcToken **endptr)
{
	if (toklist == NULL)
	{
		*endptr = toklist;
		return 0.0;
	};
	
	CalcToken *temp;
	double x = evalProduct(toklist, &temp);
	
	if (temp == toklist)
	{
		*endptr = toklist;
		return 0.0;
	};
	
	toklist = temp;
	while (toklist != NULL && (toklist->type == TOK_ADD || toklist->type == TOK_SUB))
	{
		int op = toklist->type;
		toklist = toklist->next;
		
		double y = evalProduct(toklist, &temp);
		if (temp == toklist)
		{
			*endptr = NULL;
			return 0.0;
		};
		
		toklist = temp;
		if (op == TOK_ADD)
		{
			x += y;
		}
		else
		{
			x -= y;
		};
	};
	
	*endptr = toklist;
	return x;
};

static double evalExpr(CalcToken *toklist, CalcToken **endptr)
{
	return evalSum(toklist, endptr);
};

static double parseExpr(const char *expr)
{
	CalcToken *first = NULL;
	CalcToken *last = NULL;
	
	while (*expr != 0)
	{
		char *endptr;
		double val = strtod(expr, &endptr);
		
		if (isspace(*expr))
		{
			expr++;
		}
		else if (*expr == '(')
		{
			pushToken(&first, &last, TOK_OPEN_BRACKET, 0.0);
			expr++;
		}
		else if (*expr == ')')
		{
			pushToken(&first, &last, TOK_CLOSE_BRACKET, 0.0);
			expr++;
		}
		else if (*expr == '+')
		{
			pushToken(&first, &last, TOK_ADD, 0.0);
			expr++;
		}
		else if (*expr == '-')
		{
			pushToken(&first, &last, TOK_SUB, 0.0);
			expr++;
		}
		else if (*expr == '*')
		{
			pushToken(&first, &last, TOK_MULT, 0.0);
			expr++;
		}
		else if (memcmp(expr, "×", strlen("×")) == 0)
		{
			pushToken(&first, &last, TOK_MULT, 0.0);
			expr += strlen("×");
		}
		else if (*expr == '/')
		{
			pushToken(&first, &last, TOK_DIV, 0.0);
			expr++;
		}
		else if (memcmp(expr, "÷", strlen("÷")) == 0)
		{
			pushToken(&first, &last, TOK_DIV, 0.0);
			expr += strlen("÷");
		}
		else if (endptr != expr)
		{
			pushToken(&first, &last, TOK_NUM, val);
			expr = endptr;
		}
		else
		{
			return 0.0;
		};
	};
	
	CalcToken *out;
	double result = evalExpr(first, &out);
	if (out != NULL) return 0.0;
	else return result;
};

static void doCalc()
{
	const char *expr = gwmReadTextField(txtCalc);
	double result = parseExpr(expr);
	
	char buffer[256];
	sprintf(buffer, "%f", result);
	
	gwmWriteTextField(txtCalc, buffer);
	gwmFocus(txtCalc);
};

static int commandHandler(GWMCommandEvent *ev)
{
	const char *c;
	char *buffer;
	const char *text;
	
	switch (ev->symbol)
	{
	case GWM_SYM_OK:
		doCalc();
		return GWM_EVSTATUS_OK;
	case SYM_CALC+0:
	case SYM_CALC+1:
	case SYM_CALC+2:
	case SYM_CALC+3:
	case SYM_CALC+4:
	case SYM_CALC+5:
	case SYM_CALC+6:
	case SYM_CALC+7:
	case SYM_CALC+8:
	case SYM_CALC+9:
	case SYM_CALC+10:
	case SYM_CALC+11:
	case SYM_CALC+12:
	case SYM_CALC+13:
	case SYM_CALC+15:
		c = pad[ev->symbol-SYM_CALC];
		text = gwmReadTextField(txtCalc);
		buffer = (char*) malloc(strlen(text) + strlen(c) + 1);
		sprintf(buffer, "%s%s", text, c);
		gwmWriteTextField(txtCalc, buffer);
		gwmFocus(txtCalc);
		free(buffer);
		return GWM_EVSTATUS_OK;
	default:
		return GWM_EVSTATUS_CONT;
	};
};

static int eventHandler(GWMEvent *ev, GWMWindow *win, void *context)
{
	switch (ev->type)
	{
	case GWM_EVENT_COMMAND:
		return commandHandler((GWMCommandEvent*) ev);
	default:
		return GWM_EVSTATUS_CONT;
	};
};

int main()
{
	if (gwmInit() != 0)
	{
		fprintf(stderr, "Failed to initialize GWM!\n");
		return 1;
	};
	
	GWMWindow *topWindow = gwmCreateWindow(NULL, "Calculator", GWM_POS_UNSPEC, GWM_POS_UNSPEC, 0, 0, GWM_WINDOW_NOTASKBAR | GWM_WINDOW_HIDDEN);
	assert(topWindow != NULL);

	DDISurface *surface = gwmGetWindowCanvas(topWindow);
	
	const char *error;
	DDISurface *icon = ddiLoadAndConvertPNG(&surface->format, "/usr/share/images/calc.png", &error);
	if (icon == NULL)
	{
		printf("Failed to load calculator icon: %s\n", error);
	}
	else
	{
		gwmSetWindowIcon(topWindow, icon);
	};

	GWMLayout *boxLayout = gwmCreateBoxLayout(GWM_BOX_VERTICAL);
	
	GWMWindowTemplate wt;
	wt.wtComps = GWM_WTC_MENUBAR | GWM_WTC_TOOLBAR;
	wt.wtWindow = topWindow;
	wt.wtBody = boxLayout;
	gwmCreateTemplate(&wt);
	
	GWMWindow *menubar = wt.wtMenubar;
	
	GWMMenu *menuFile = gwmCreateMenu();
	gwmMenubarAdd(menubar, "File", menuFile);
	
	gwmMenuAddCommand(menuFile, GWM_SYM_EXIT, NULL, NULL);
	
	txtCalc = gwmNewTextField(topWindow);
	gwmBoxLayoutAddWindow(boxLayout, txtCalc, 0, 0, GWM_BOX_FILL);
	gwmSetTextFieldAlignment(txtCalc, DDI_ALIGN_RIGHT);
	
	GWMLayout *grid = gwmCreateGridLayout(4);
	gwmBoxLayoutAddLayout(boxLayout, grid, 1, 0, GWM_BOX_FILL);
	
	int i;
	for (i=0; i<4*4; i++)
	{
		int symbol = SYM_CALC+i;
		if (i == 14) symbol = GWM_SYM_OK;		/* '=' button */
		
		GWMButton *btn = gwmCreateButtonWithLabel(topWindow, symbol, pad[i]);
		gwmGridLayoutAddWindow(grid, btn, 1, 1);
	};
	
	gwmFit(topWindow);
	gwmPushEventHandler(topWindow, eventHandler, NULL);
	gwmSetWindowFlags(topWindow, GWM_WINDOW_MKFOCUSED | GWM_WINDOW_RESIZEABLE);
	gwmMainLoop();
	gwmQuit();
	return 0;
};
