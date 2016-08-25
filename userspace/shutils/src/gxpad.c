/*
	Glidix Shell Utilities

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

#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <inttypes.h>
#include <string.h>
#include <termios.h>
#include <signal.h>
#include <errno.h>
#include <fcntl.h>

struct termios tc;
volatile int ctrlc = 0;

/**
 * Name of the open file.
 */
const char *filename = NULL;

/**
 * An array of lines in the file being edited.
 */
char **lines = NULL;
int numLines = 0;

/**
 * Position of the editor cursor.
 */
int cursorLine = 0;
int cursorPos = 0;

/**
 * Index of the first line to render.
 */
int scrollLine = 0;
int scrollX = 0;

void clearScreen()
{
	write(1, "\e!", 2);
};

void setColor(uint8_t col)
{
	uint8_t seq[3] = "\e\"?";
	seq[2] = col;
	write(1, seq, 3);
};

void setCursor(uint8_t x, uint8_t y)
{
	uint8_t seq[4] = "\e#??";
	seq[2] = x;
	seq[3] = y;
	write(1, seq, 4);
};

void on_signal(int sig, siginfo_t *si, void *ignore)
{
	ctrlc = 1;
};

void renderEditor()
{
	char bar[80];
	memset(bar, ' ', 80);
	
	clearScreen();
	setCursor(0, 0);
	setColor(0x10);
	write(1, bar, 80);
	
	setCursor(0, 24);
	write(1, bar, 80);
	
	setCursor(0, 24);
	printf("Ctrl-C to open menu");
	fflush(stdout);
	
	setCursor(0, 0);
	printf("File: %s", filename);
	fflush(stdout);
	
	setColor(0x07);
	setCursor(0, 1);
	
	char linebuf[80];
	
	uint8_t effX = 0, effY = 1;
	
	int i;
	for (i=0; i<23; i++)
	{
		int lineno = scrollLine + i;
		memset(linebuf, ' ', 80);
		
		if (lineno < numLines)
		{
			if (scrollX < strlen(lines[lineno]))
			{
				size_t count = strlen(&lines[lineno][scrollX]);
				if (count > 80)
				{
					count = 80;
				};
			
				memcpy(linebuf, &lines[lineno][scrollX], count);
			};
		};
		
		write(1, linebuf, 80);
	};
	
	setCursor(cursorPos-scrollX, 1+(cursorLine-scrollLine));
};

void adjustX()
{
	if (cursorLine == numLines)
	{
		scrollX = 0;
		cursorPos = 0;
	}
	else
	{
		if (cursorPos > strlen(lines[cursorLine]))
		{
			cursorPos = strlen(lines[cursorLine]);
			if ((scrollX+79) < cursorPos)
			{
				scrollX = cursorPos - 79;
			};
			
			if (scrollX > cursorPos)
			{
				scrollX = cursorPos;
			};
		};
	};
};

const char *saveFile()
{
	FILE *fp = fopen(filename, "wb");
	if (fp == NULL)
	{
		return strerror(errno);
	};
	
	int i;
	for (i=0; i<numLines; i++)
	{
		if (fprintf(fp, "%s\n", lines[i]) < 0)
		{
			const char *out = strerror(errno);
			fclose(fp);
			return out;
		};
	};
	
	fclose(fp);
	return "File saved.";
};

const char *quitEditor()
{
	printf("\nSave before exiting? (Y-yes/N-no/C-cancel) ");
	fflush(stdout);
	
	while (1)
	{
		char c;
		read(0, &c, 1);
		
		const char *state = "";
		
		switch (c)
		{
		case 'y':
		case 'Y':
			state = saveFile();
			if (strcmp(state, "File saved.") == 0)
			{
				clearScreen();
				tc.c_lflag |= ECHO | ECHONL | ICANON;
				tcsetattr(0, TCSANOW, &tc);
				exit(0);
			}
			else
			{
				return state;
			};
			break;
		case 'N':
		case 'n':
			clearScreen();
			tc.c_lflag |= ECHO | ECHONL | ICANON;
			tcsetattr(0, TCSANOW, &tc);
			exit(0);
			break;
		case 'C':
		case 'c':
			return "Cancelled.";
		};
	};
};

void renderMenu(const char *msg)
{
	clearScreen();
	
	printf("GXPAD MENU\n");
	printf(" 0. Return to editor\n");
	printf(" 1. Save file\n");
	printf(" 2. Quit\n");
	printf("\n%s\nOption: ", msg);
	fflush(stdout);
};

void gotoMenu()
{
	renderMenu("");
	
	while (1)
	{
		char c = 0;
		read(0, &c, 1);
	
		switch (c)
		{
		case '0':
			ctrlc = 0;
			return;
		case '1':
			renderMenu(saveFile());
			break;
		case '2':
			renderMenu(quitEditor());
			break;
		};
	};
};

int main(int argc, char *argv[])
{
	if (argc != 2)
	{
		fprintf(stderr, "USAGE:\t%s <filename>\n", argv[0]);
		fprintf(stderr, "\tA simple console-based text editor.\n");
		fprintf(stderr, "\tThe file must exist; you can create it using 'touch'.\n");
		return 1;
	};
	
	struct stat st;
	if (stat(argv[1], &st) != 0)
	{
		fprintf(stderr, "%s: stat %s: %s\n", argv[0], argv[1], strerror(errno));
		return 1;
	};
	
	filename = argv[1];
	
	/**
	 * Read the entire file into memory before slicing it into individual lines.
	 */
	int fd = open(argv[1], O_RDONLY);
	if (fd == -1)
	{
		fprintf(stderr, "%s: %s: %s\n", argv[0], argv[1], strerror(errno));
		return 1;
	};
	
	char *data = (char*) malloc(st.st_size+1);
	read(fd, data, st.st_size);
	data[st.st_size] = 0;
	close(fd);
	
	if (data[st.st_size-1] == '\n')
	{
		// remove ending empty line
		data[st.st_size] = 0;
	};
	
	char *parse = data;
	while (parse != NULL)
	{
		char *delim = strchr(parse, '\n');
		if (delim != NULL) *delim = 0;
		lines = realloc(lines, sizeof(char*)*(numLines+1));
		lines[numLines++] = strdup(parse);
		
		if (delim != NULL)
		{
			parse = delim+1;
		}
		else
		{
			parse = NULL;
		};
	};
	
	free(data);
	
	struct sigaction sa;
	sa.sa_sigaction = on_signal;
	sa.sa_flags = SA_SIGINFO;
	if (sigaction(SIGINT, &sa, NULL) != 0)
	{
		perror("sigaction SIGINT");
		return 1;
	};

	tcgetattr(0, &tc);
	tc.c_lflag &= ~(ECHO | ECHONL | ICANON);
	tcsetattr(0, TCSANOW, &tc);

	renderEditor();
	
	while (1)
	{
		char c;
		int state = read(0, &c, 1);
		
		if (ctrlc)
		{
			read(0, &c, 1);		// delete the Ctrl-C
			gotoMenu();
			renderEditor();
		}
		else if (state == -1)
		{
			// NOP
		}
		else
		{
			if ((c & 0x80) == 0)
			{
				if (cursorLine == numLines)
				{
					lines = realloc(lines, sizeof(char*)*(numLines+1));
					lines[numLines++] = (char*) malloc(1);
					lines[cursorLine][0] = 0;
				};
				
				if (c == '\n')
				{
					char *orgline = lines[cursorLine];
					
					char *leftLine = (char*) malloc(cursorPos+1);
					leftLine[cursorPos] = 0;
					memcpy(leftLine, orgline, cursorPos);
					
					char *rightLine = (char*) malloc(strlen(orgline)-cursorPos+1);
					rightLine[strlen(orgline)-cursorPos] = 0;
					memcpy(rightLine, &orgline[cursorPos], strlen(orgline)-cursorPos);
					
					free(orgline);
					
					char **newLines = (char**) malloc(sizeof(char*)*(numLines+1));
					memcpy(newLines, lines, sizeof(char*)*(cursorLine));
					newLines[cursorLine] = leftLine;
					newLines[cursorLine+1] = rightLine;
					memcpy(&newLines[cursorLine+2], &lines[cursorLine+1], sizeof(char*)*(numLines-cursorLine-1));
					
					free(lines);
					lines = newLines;
					numLines++;
					cursorLine++;
					cursorPos = 0;
					scrollX = 0;
					
					if ((cursorLine-scrollLine) > 20)
					{
						scrollLine++;
					};
				}
				else if (c == '\b')
				{
					if (cursorPos == 0)
					{
						if (cursorLine != 0)
						{
							size_t oldsize = strlen(lines[cursorLine-1]);
							lines[cursorLine-1] = realloc(lines[cursorLine-1], oldsize+strlen(lines[cursorLine])+1);
							memcpy(&lines[cursorLine-1][oldsize], lines[cursorLine], strlen(lines[cursorLine]));
							lines[cursorLine-1][oldsize+strlen(lines[cursorLine])] = 0;
							
							memmove(&lines[cursorLine], &lines[cursorLine+1], numLines-cursorLine);
							numLines--;
							
							cursorLine--;
							cursorPos += oldsize;
							
							if ((scrollX+79) < cursorPos)
							{
								scrollX = cursorPos - 79;
							};
			
							if (scrollX > cursorPos)
							{
								scrollX = cursorPos;
							};
						};
					}
					else
					{
						cursorPos--;
						
						size_t oldsize = strlen(lines[cursorLine]);
						memmove(&lines[cursorLine][cursorPos], &lines[cursorLine][cursorPos+1], oldsize-cursorPos);
						lines[cursorLine][oldsize-1] = 0;
						
						if (scrollX > cursorPos)
						{
							scrollX = cursorPos;
						};
					};
				}
				else
				{
				
					char *oldline = lines[cursorLine];
					char *newline = (char*) malloc(strlen(oldline)+2);
				
					memcpy(newline, oldline, cursorPos);
					newline[cursorPos] = c;
					memcpy(&newline[cursorPos+1], &oldline[cursorPos], strlen(oldline)-cursorPos);
					newline[strlen(oldline)+1] = 0;
				
					lines[cursorLine] = newline;
					free(oldline);
					cursorPos++;
					
					if ((cursorPos-scrollX) >= 80)
					{
						scrollX = cursorPos - 79;
					};
				};
			}
			else if ((uint8_t)c == 0x8B)
			{
				// up arrow
				if (cursorLine != 0)
				{
					cursorLine--;
					if ((cursorLine-scrollLine) < 5)
					{
						scrollLine--;
						if (scrollLine < 0) scrollLine = 0;
					};
					
					adjustX();
				};
			}
			else if ((uint8_t)c == 0x8C)
			{
				// down arrow
				if (cursorLine < numLines)
				{
					cursorLine++;
					if (((scrollLine+25)-cursorLine) < 5)
					{
						scrollLine++;
					};
					
					adjustX();
				};
			}
			else if ((uint8_t)c == 0x8D)
			{
				// left arrow
				if (cursorPos != 0)
				{
					cursorPos--;
					
					if (scrollX > cursorPos)
					{
						scrollX = cursorPos;
					};
				};
			}
			else if ((uint8_t)c == 0x8E)
			{
				// right arrow
				if (cursorLine != numLines)
				{
					if (cursorPos != strlen(lines[cursorLine]))
					{
						cursorPos++;
						if ((scrollX+79) < cursorPos)
						{
							scrollX = cursorPos - 79;
						};
					};
				};
			};
			
			renderEditor();
		};
	};
	
	clearScreen();
	tc.c_lflag |= ECHO | ECHONL | ICANON;
	tcsetattr(0, TCSANOW, &tc);
	return 0;
};
