/*
	Glidix Shell Utilities

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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

char *progName;

const char *stylesheet = "\
body\n\
{\n\
	margin-left: 1cm;\n\
	margin-right: 1cm;\n\
}\n\
\n\
.header\n\
{\n\
	text-align: center;\n\
	font-weight: bold;\n\
}\n\
\n\
h1, h2, h3\n\
{\n\
	font-family: sans-serif;\n\
	font-weight: bold;\n\
	color: #008800;\n\
}\n\
\n\
p, li\n\
{\n\
	line-height: 150%;\n\
	color: #333333;\n\
	font-size: 17px;\n\
	font-family: sans-serif;\n\
	letter-spacing: 1px;\n\
}\n\
\n\
pre\n\
{\n\
	margin-left: 0.5cm;\n\
	margin-right: 0.5cm;\n\
	background-color: #DDDDDD;\n\
	font-size: 17px;\n\
	line-height: 140%;\n\
	font-family: monospace;\n\
	\n\
	white-space: pre-wrap;\n\
	white-space: -moz-pre-wrap;\n\
	white-space: -pre-wrap;\n\
	white-space: -o-pre-wrap;\n\
	word-wrap: break-word;\n\
}\n\
\n\
pre .ref\n\
{\n\
	font-style: italic;\n\
}\n\
\n\
p .ref, li .ref\n\
{\n\
	background-color: #DDDDDD;\n\
	font-family: monospace;\n\
	font-style: italic;\n\
}\n\
\n\
.emph\n\
{\n\
	color: black;\n\
	font-weight: bold;\n\
	font-family: monospace;\n\
}\n\
\n\
.search\n\
{\n\
	background-color: #999900;\n\
	color: #ffffff;\n\
}\n\
\n\
a:link {color: #0000AA; text-decoration: none;}\n\
a:visited {color: #0000AA; text-decoration: none;}\n\
a:active {color: #0000AA; text-decoration: none;}\n\
a:hover {color: #AA0000; text-decoration: underline;}\n\
";

const char *javascript = "\
function render()\n\
{\n\
	var filter = document.getElementById(\"filter\").value;\n\
	var text = \"<ol>\";\n\
	var i;\n\
	for (i=1; i<=6; i++)\n\
	{\n\
		text += \"<li>\" + chapterNames[i-1] + \"<ul>\";\n\
		var j;\n\
		for (j=0; j<pages[i].length; j++)\n\
		{\n\
			var label;\n\
			var pos = pages[i][j].indexOf(filter);\n\
			if ((pos !== -1) || (filter == \"\"))\n\
			{\n\
				if (filter == \"\")\n\
				{\n\
					label = pages[i][j] + \"(\" + i + \")\";\n\
				}\n\
				else\n\
				{\n\
					var firstBit = pages[i][j].substring(0, pos);\n\
					var middleBit = pages[i][j].substring(pos, pos+filter.length);\n\
					var lastBit = pages[i][j].substring(pos+filter.length);\n\
					label = firstBit + '<span class=\"search\">' + middleBit + '</span>' + lastBit + \"(\" + i + \")\";\n\
				};\n\
				text += '<li><a href=\"' + pages[i][j] + \".\" + i + \".html\" + '\">' + label + \"</a></li>\";\n\
			};\n\
		};\n\
		text += \"</ul>\";\n\
	};\n\
	text += \"</ol>\";\n\
	document.getElementById(\"toc\").innerHTML = text;\n\
};\n\
\n\
window.onload = render;\n\
";

void usage()
{
	fprintf(stderr, "USAGE:\t%s --html <input-file> <output-file> # Convert sysman page to HTML\n", progName);
	fprintf(stderr, "USAGE:\t%s --index <input-html-files> # Generate index and CSS for HTML pages\n", progName);
};

void writeParagraph(const char *scan, FILE *out, int codeMode)
{
	int refState = 0;
	int emphState = 0;
	
	for (; *scan!=0; scan++)
	{
		if (*scan == '&')
		{
			fprintf(out, "&amp;");
		}
		else if (*scan == '<')
		{
			fprintf(out, "&lt;");
		}
		else if (*scan == '>')
		{
			fprintf(out, "&gt;");
		}
		else if ((*scan == '[') && (!codeMode))
		{
			char buffer[1024];
			scan++;
			char *put = buffer;
			
			while (*scan != ']')
			{
				*put++ = *scan++;
			};
			
			*put = 0;
			
			char *dot = strchr(buffer, '.');
			if (dot == NULL) continue;
			
			*dot = 0;
			int chapter;
			if (sscanf(dot+1, "%d", &chapter) != 1) continue;
			
			fprintf(out, "<a href=\"%s.%d.html\">%s(%d)</a>", buffer, chapter, buffer, chapter);
		}
		else if (*scan == '\'')
		{
			refState = !refState;
			
			if (refState)
			{
				fprintf(out, "<span class=\"ref\">");
			}
			else
			{
				fprintf(out, "</span>");
			};
		}
		else if ((*scan == '*') && (!codeMode))
		{
			emphState = !emphState;
			
			if (emphState)
			{
				fprintf(out, "<span class=\"emph\">");
			}
			else
			{
				fprintf(out, "</span>");
			};
		}
		else if (*scan == '\\')
		{
			scan++;
			fprintf(out, "%c", *scan);
		}
		else
		{
			fprintf(out, "%c", *scan);
		};
	};
};

char linebuf[65536];
int genHTML(const char *infile, const char *outfile)
{
	char pagename[256];
	if (strrchr(infile, '/') != NULL)
	{
		strcpy(pagename, strrchr(infile, '/') + 1);
	}
	else
	{
		strcpy(pagename, infile);
	};
	
	int chapter;
	char *dot = strchr(pagename, '.');
	if (dot == NULL)
	{
		fprintf(stderr, "%s: invalid page name: %s\n", progName, infile);
		return 1;
	};
	
	*dot = 0;
	if (sscanf(dot+1, "%d", &chapter) != 1)
	{
		fprintf(stderr, "%s: failed to parse chapter number for %s\n", progName, infile);
		return 1;
	};
	
	FILE *in = fopen(infile, "r");
	if (in == NULL)
	{
		fprintf(stderr, "%s: cannot open %s: %s\n", progName, infile, strerror(errno));
		return 1;
	};
	
	FILE *out = fopen(outfile, "w");
	if (out == NULL)
	{
		fprintf(stderr, "%s: cannot write to %s: %s\n", progName, outfile, strerror(errno));
		return 1;
	};
	
	fprintf(out, "<!DOCTYPE html>\n<html>\n\t<head>\n");
	fprintf(out, "\t\t<title>%s(%d) - GLIDIX MANUAL</title>\n", pagename, chapter);
	fprintf(out, "\t\t<meta http-equiv=\"Content-Type\" content=\"text/html; charset=utf-8\"/>\n");
	fprintf(out, "\t\t<link rel=\"stylesheet\" type=\"text/css\" href=\"sysman.css\"/>\n");
	fprintf(out, "\t</head>\n\n\t<body>\n");
	
	fprintf(out, "\t\t<div class=\"header\">GLIDIX MANUAL</div>\n");
	fprintf(out, "\t\t<hr/>\n");
	
	while (fgets(linebuf, 65536, in) != NULL)
	{
		char *newline = strchr(linebuf, '\n');
		if (newline == NULL)
		{
			fprintf(stderr, "%s: line too long\n", progName);
			return 1;
		};
		*newline = 0;
		
		if (memcmp(linebuf, ">>", 2) == 0)
		{
			fprintf(out, "\t\t<h2>%s</h2>\n", &linebuf[2]);
		}
		else if (linebuf[0] == '>')
		{
			fprintf(out, "\t\t<h1>%s</h1>\n", &linebuf[1]);
		}
		else if (memcmp(linebuf, "\\*", 2) == 0)
		{
			fprintf(out, "\t\t<ul>\n\t\t\t<li>\n");
			writeParagraph(&linebuf[2], out, 0);
			fprintf(out, "\n\t\t\t</li>\n\t\t</ul>\n");
		}
		else if (linebuf[0] == '\t')
		{
			fprintf(out, "\t\t<pre>\n");
			writeParagraph(&linebuf[1], out, 1);
			fprintf(out, "\n");
			
			while (1)
			{
				if (fgets(linebuf, 65536, in) == NULL)
				{
					break;
				};
				
				if (linebuf[0] != '\t')
				{
					if (linebuf[0] != '\n')
					{
						fprintf(stderr, "%s: %s: warning: code blocks should end with an empty line!\n",
							progName, infile);
					};
					
					break;
				};
				
				writeParagraph(&linebuf[1], out, 1);	// we did not remove newline from this one so it OK
			};
			
			fprintf(out, "</pre>\n");
		}
		else
		{
			fprintf(out, "\t\t<p>\n");
			writeParagraph(linebuf, out, 0);
			fprintf(out, "\n\t\t</p>\n");
		};
	};
	
	fclose(in);
	fprintf(out, "\t</body>\n</html>\n");
	fclose(out);
	
	return 0;
};

const char *chapterNames[] = {
	NULL,		/* chapter 0 */
	"Commands",
	"System calls and library functions",
	"Special files",
	"Kernel modules and drivers",
	"Kernel-mode API",
	"General concepts"
};

char **chapters[7];	/* including "chapter 0" */
int chapterSizes[7];	/* including "chapter 0" */

int doIndex(int count, char *files[])
{
	FILE *fp = fopen("sysman.css", "w");
	if (fp == NULL)
	{
		fprintf(stderr, "%s: cannot write to sysman.css: %s\n", progName, strerror(errno));
		return 1;
	};
	
	fprintf(fp, "%s\n", stylesheet);
	fclose(fp);
	
	fp = fopen("index.html", "w");
	if (fp == NULL)
	{
		fprintf(stderr, "%s: cannot write to index.html: %s\n", progName, strerror(errno));
		return 1;
	};
	
	int i;
	for (i=0; i<count; i++)
	{
		char pagename[1024];
		int chapter;
		
		char *lastSlash = strrchr(files[i], '/');
		if (lastSlash != NULL)
		{
			strcpy(pagename, lastSlash+1);
		}
		else
		{
			strcpy(pagename, files[i]);
		};
		
		char *dot = strchr(pagename, '.');
		if (dot == NULL)
		{
			fprintf(stderr, "%s: invalid page name for index: %s\n", progName, files[i]);
			return 1;
		};
		
		*dot = 0;
		if (sscanf(dot+1, "%d", &chapter) != 1)
		{
			fprintf(stderr, "%s: invalid page name for index (no chapter number): %s\n", progName, files[i]);
			return 1;
		};
		
		int index = chapterSizes[chapter]++;
		chapters[chapter] = (char**) realloc(chapters[chapter], sizeof(char*) * chapterSizes[chapter]);
		chapters[chapter][index] = strdup(pagename);
	};
	
	fprintf(fp, "<!DOCTYPE html>\n");
	fprintf(fp, "<html>\n\t<head>\n");
	fprintf(fp, "\t\t<title>GLIDIX MANUAL INDEX</title>\n");
	fprintf(fp, "\t\t<meta http-equiv=\"Content-Type\" content=\"text/html; charset=utf-8\"/>\n");
	fprintf(fp, "\t\t<link rel=\"stylesheet\" type=\"text/css\" href=\"sysman.css\"/>\n");
	fprintf(fp, "\t\t<script>\n");
	fprintf(fp, "var chapterNames = [\n");
	for (i=1; i<=6; i++)
	{
		fprintf(fp, "\t\"%s\",\n", chapterNames[i]);
	};
	fprintf(fp, "];\n\n");
	
	fprintf(fp, "var pages = [\n");
	for (i=0; i<=6; i++)
	{
		fprintf(fp, "\t[\n");
		
		int j;
		for (j=0; j<chapterSizes[i]; j++)
		{
			fprintf(fp, "\t\t\"%s\",\n", chapters[i][j]);
		};
		
		fprintf(fp, "\t],\n\n");
	};
	
	fprintf(fp, "];\n");
	fprintf(fp, "%s", javascript);
	fprintf(fp, "\t\t</script>\n");
	fprintf(fp, "\t</head>\n\t<body>\n");

	fprintf(fp, "\t\t<div class=\"header\">GLIDIX MANUAL</div>\n");
	fprintf(fp, "\t\t<hr/>\n");
	fprintf(fp, "\t\t<h1>Table of Contents</h1>\n");
#if 0
	fprintf(fp, "\t\t<ol>\n");
	
	for (i=1; i<=6; i++)
	{
		fprintf(fp, "\t\t\t<li>%s<ul>\n", chapterNames[i]);
		
		int j;
		for (j=0; j<chapterSizes[i]; j++)
		{
			fprintf(fp, "\t\t\t\t<li><a href=\"%s.%d.html\">%s(%d)</a></li>\n", chapters[i][j], i, chapters[i][j], i);
		};
		
		fprintf(fp, "\t\t\t</ul></li>\n");
	};
	
	fprintf(fp, "\t\t</ol>\n");
#endif

	fprintf(fp, "\t\t<p>Search: <input type=\"text\" id=\"filter\" oninput=\"render()\"/>\n");
	fprintf(fp, "\t\t<div id=\"toc\"></div>\n");
	fprintf(fp, "\t</body>\n");
	fprintf(fp, "</html>\n");
	fclose(fp);
	
	return 0;
};

int main(int argc, char *argv[])
{
	progName = argv[0];
	
	if (argc < 2)
	{
		usage();
		return 1;
	};
	
	if (strcmp(argv[1], "--html") == 0)
	{
		if (argc != 4)
		{
			usage();
			return 1;
		};
		
		return genHTML(argv[2], argv[3]);
	}
	else if (strcmp(argv[1], "--index") == 0)
	{
		return doIndex(argc-2, &argv[2]);
	}
	else
	{
		usage();
		return 1;
	};
};
