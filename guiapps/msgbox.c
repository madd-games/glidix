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
#include <libddi.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

int main(int argc, char *argv[])
{
	if (gwmInit() != 0)
	{
		fprintf(stderr, "%s: gwmInit failed\n", argv[0]);
		return 1;
	};
	
	if (argc < 3)
	{
		char *errmsg;
		asprintf(&errmsg, "SYNTAX: %s <caption> <text> [options...]\n"
			"\nDisplay a message box. The following options are available:\n"
			"\n--error\t\tDisplay an error icon.\n"
			"--info\t\tDisplay an info icon.\n"
			"--warn\t\tDisplay a warning icon.\n"
			"--quest\t\tDisplay a question icon.\n"
			"--success\t\tDisplay a success icon.\n"
			"\n--ok\t\tShow only an OK button.\n"
			"--ok-cancel\tShow OK/Cancel buttons.\n"
			"--yes-no\t\tDisplay Yes/No buttons.\n"
			"--yes-no-cancel\tDisplay Yes/No/Cancel buttons.\n", argv[0]);
		gwmMessageBox(NULL, argv[0], errmsg, GWM_MBICON_INFO | GWM_MBUT_OK);
		return 1;
	};
	
	const char *caption = argv[1];
	const char *text = argv[2];
	int flags = 0;
	
	int i;
	for (i=3; i<argc; i++)
	{
		const char *spec = argv[i];
		
		if (strcmp(spec, "--error") == 0)
		{
			flags |= GWM_MBICON_ERROR;
		}
		else if (strcmp(spec, "--info") == 0)
		{
			flags |= GWM_MBICON_INFO;
		}
		else if (strcmp(spec, "--warn") == 0)
		{
			flags |= GWM_MBICON_WARN;
		}
		else if (strcmp(spec, "--quest") == 0)
		{
			flags |= GWM_MBICON_QUEST;
		}
		else if (strcmp(spec, "--success") == 0)
		{
			flags |= GWM_MBICON_SUCCESS;
		}
		else if (strcmp(spec, "--ok") == 0)
		{
			flags |= GWM_MBUT_OK;
		}
		else if (strcmp(spec, "--ok-cancel") == 0)
		{
			flags |= GWM_MBUT_OKCANCEL;
		}
		else if (strcmp(spec, "--yes-no") == 0)
		{
			flags |= GWM_MBUT_YESNO;
		}
		else if (strcmp(spec, "--yes-no-cancel") == 0)
		{
			flags |= GWM_MBUT_YESNOCANCEL;
		}
		else
		{
			char *errmsg;
			asprintf(&errmsg, "Unrecognised command-line option: %s", spec);
			gwmMessageBox(NULL, argv[0], errmsg, GWM_MBICON_ERROR | GWM_MBUT_OK);
			return 1;
		};
	};
	
	int answer = gwmMessageBox(NULL, caption, text, flags);
	switch (answer)
	{
	case GWM_SYM_YES:
		printf("yes\n");
		return 0;
	case GWM_SYM_NO:
		printf("no\n");
		return 0;
	case GWM_SYM_OK:
		printf("ok\n");
		return 0;
	case GWM_SYM_CANCEL:
		printf("cancel\n");
		return 0;
	default:
		printf("undefined\n");
		return 0;
	};
};
