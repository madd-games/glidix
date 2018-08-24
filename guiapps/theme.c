/*
	Glidix GUI

	Copyright (c) 2014-2017, Madd Games.
	All rights reserved.
	
	Redistribution and use in source and binary forms, with or without
	modification, are permitted provided that the following conditions are met:
	
	* Redistributions of source code must retain the above copyright notice, this
	  list of conditions and the following disclaimer.
	
	* Redistributions in binary form must reproduce the above copyright notice,
	  this list of conditions and the following disclaimer in the documentationx
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

#include <stdio.h>
#include <stdlib.h>
#include <libgwm.h>
#include <libddi.h>
#include <string.h>
#include <errno.h>

const char *progName;

void usage()
{
	fprintf(stderr, "USAGE:\t%s <command> [arguments...]\n", progName);
	fprintf(stderr, "\tControls theme information.\n");
	fprintf(stderr, "\n\t%s set image <name> <path>\n", progName);
	fprintf(stderr, "\tSet a theme image to the specified path.\n");
	fprintf(stderr, "\n\t%s set color <name> <value>\n", progName);
	fprintf(stderr, "\tSet a theme color to the specified value.\n");
	fprintf(stderr, "\n\t%s load <theme-path>\n", progName);
	fprintf(stderr, "\tLoad a theme from the given theme file.\n");
};

int themeSet(char *type, char *name, char *value)
{
	if (strcmp(type, "image") == 0)
	{
		DDISurface *surface = (DDISurface*) gwmGetThemeProp(name, GWM_TYPE_SURFACE, NULL);
		if (surface == NULL)
		{
			fprintf(stderr, "%s: no image property named `%s'\n", progName, name);
			return 1;
		};
		
		const char *error;
		DDISurface *image = ddiLoadAndConvertPNG(&surface->format, value, &error);
		if (image == NULL)
		{
			fprintf(stderr, "%s: failed to load image `%s': %s\n", progName, value, error);
			return 1;
		};
		
		ddiOverlay(image, 0, 0, surface, 0, 0, image->width, image->height);
		gwmRetheme();
		return 0;
	}
	else if (strcmp(type, "color") == 0)
	{
		DDIColor *color = (DDIColor*) gwmGetThemeProp(name, GWM_TYPE_COLOR, NULL);
		if (color == NULL)
		{
			fprintf(stderr, "%s: no color property named `%s'\n", progName, name);
			return 1;
		};
		
		uint8_t alpha = 0xFF;
		char *spec = value;
		char *colonPos = strchr(spec, ':');
		if (colonPos != NULL)
		{
			*colonPos = 0;
			char *endptr;
			char *alphaSpec = colonPos + 1;
			
			uint64_t alpha64 = strtoul(alphaSpec, &endptr, 0);
			if (*endptr != 0)
			{
				fprintf(stderr, "%s: invalid alpha value `%s'\n", progName, alphaSpec);
				return 1;
			};
			
			if (alpha64 > 0xFF)
			{
				fprintf(stderr, "%s: invalid alpha value `%s'\n", progName, alphaSpec);
				return 1;
			};
			
			alpha = (uint8_t) alpha64;
		};
		
		if (ddiParseColor(spec, color) != 0)
		{
			fprintf(stderr, "%s: invalid color `%s'\n", progName, value);
			return 1;
		};
		
		color->alpha = alpha;
		gwmRetheme();
		return 0;
	}
	else
	{
		fprintf(stderr, "%s: unknown theme property type `%s'\n", progName, type);
		return 1;
	};
};

int themeLoad(const char *filename)
{
	int status = 0;
	FILE *fp = fopen(filename, "r");
	if (fp == NULL)
	{
		fprintf(stderr, "%s: cannot open %s: %s\n", progName, filename, strerror(errno));
		return 1;
	};
	
	char linebuf[1024];
	char currentNamespace[1024];
	currentNamespace[0] = 0;
	
	int lineno = 0;
	while (fgets(linebuf, 1024, fp) != NULL)
	{
		lineno++;
		char *commentPos = strchr(linebuf, '\'');
		if (commentPos != NULL) *commentPos = 0;
		
		char *cmd = strtok(linebuf, " \t\n");
		if (cmd == NULL) continue;
		
		if (strcmp(cmd, "namespace") == 0)
		{
			char *name = strtok(NULL, " \t\n");
			if (name == NULL)
			{
				fprintf(stderr, "%s:%d: error: expected namespace name\n", filename, lineno);
				status = 1;
				break;
			};
			
			char *brace = strtok(NULL, " \t\n");
			if (brace == NULL || strcmp(brace, "{") != 0)
			{
				fprintf(stderr, "%s:%d: error: expected '{'\n", filename, lineno);
				status = 1;
				break;
			};
			
			if (strtok(NULL, " \t\n") != NULL)
			{
				fprintf(stderr, "%s:%d: error: expected end of line after '{'\n", filename, lineno);
				status = 1;
				break;
			};
			
			if (currentNamespace[0] == 0)
			{
				strcpy(currentNamespace, name);
			}
			else
			{
				strcat(currentNamespace, ".");
				strcat(currentNamespace, name);
			};
		}
		else if (strcmp(cmd, "}") == 0)
		{
			if (strtok(NULL, " \t\n") != NULL)
			{
				fprintf(stderr, "%s:%d: error: expected end of line after '}'\n", filename, lineno);
				status = 1;
				break;
			};
			
			if (currentNamespace[0] == 0)
			{
				fprintf(stderr, "%s:%d: error: '}' without initial 'namespace'\n", filename, lineno);
				status = 1;
				break;
			};
			
			char *dotPos = strrchr(currentNamespace, '.');
			if (dotPos == NULL)
			{
				currentNamespace[0] = 0;
			}
			else
			{
				*dotPos = 0;
			};
		}
		else
		{
			if (currentNamespace[0] == 0)
			{
				fprintf(stderr, "%s:%d: error: property setting outside of namespaces\n", filename, lineno);
				status = 1;
				break;
			};
			
			char *name = strtok(NULL, " \t\n");
			if (name == NULL)
			{
				fprintf(stderr, "%s:%d: error: expected property name\n", filename, lineno);
				status = 1;
				break;
			};
			
			char *value = strtok(NULL, " \t\n");
			if (value == NULL)
			{
				fprintf(stderr, "%s:%d: error: expected property value\n", filename, lineno);
				status = 1;
				break;
			};
			
			if (strtok(NULL, " \t\n") != NULL)
			{
				fprintf(stderr, "%s:%d: error: expected end of line after property value\n", filename, lineno);
				status = 1;
				break;
			};
			
			char fullname[1024];
			sprintf(fullname, "%s.%s", currentNamespace, name);
			
			status = status || themeSet(cmd, fullname, value);
		};
	};
	
	fclose(fp);
	return status;
};

int main(int argc, char *argv[])
{
	if (geteuid() != 0)
	{
		fprintf(stderr, "%s: only root can edit theme information\n", argv[0]);
		return 1;
	};
	
	progName = argv[0];
	
	if (argc < 2)
	{
		usage();
		return 1;
	};
	
	if (gwmInit() != 0)
	{
		fprintf(stderr, "%s: failed to initialize GWM\n", argv[0]);
		return 1;
	};
	
	if (strcmp(argv[1], "set") == 0)
	{
		if (argc != 5)
		{
			usage();
			return 1;
		};
		
		return themeSet(argv[2], argv[3], argv[4]);
	}
	else if (strcmp(argv[1], "load") == 0)
	{
		if (argc != 3)
		{
			usage();
			return 1;
		};
		
		themeLoad("/usr/share/themes/GlidixGreen.thm");
		int status = themeLoad(argv[2]);
		gwmRetheme();
		return status;
	}
	else
	{
		fprintf(stderr, "%s: unknown command `%s'\n", progName, argv[1]);
		fprintf(stderr, "hint: run `%s' without command-line arguments for help.\n", argv[0]);
		return 1;
	};
	
	return 0;
};
