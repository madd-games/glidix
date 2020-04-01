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
#include <assert.h>
#include <pwd.h>
#include <unistd.h>

/**
 * Entry in the high score table.
 */
typedef struct
{
	/**
	 * Name of the player, limited to 31 characters.
	 */
	char				name[32];
	
	/**
	 * Score obtained by player.
	 */
	int				score;
} HighScoreEntry;

void gwmHighScore(const char *gameName, int score)
{
	char buffer[0x1000];
	struct passwd pwdbuf;
	struct passwd* pwd;
	
	if (getpwuid_r(geteuid(), &pwdbuf, buffer, 0x1000, &pwd) != 0)
	{
		fprintf(stderr, "libgwm: getpwuid_r failed!\n");
		abort();
	};
	
	// first, initialize the score table to defaults.
	HighScoreEntry table[10];
	int i;
	for (i=0; i<10; i++)
	{
		memset(table[i].name, '-', 31);
		table[i].name[31] = 0;
		table[i].score = 0;
	};
	
	// load the real table if it already exists
	char *tablePath;
	asprintf(&tablePath, "%s/.%s.score", pwd->pw_dir, gameName);
	
	FILE *fp = fopen(tablePath, "rb");
	if (fp != NULL)
	{
		fread(table, sizeof(HighScoreEntry), 10, fp);
		fclose(fp);
	};
	
	// sanitize the table
	for (i=0; i<10; i++)
	{
		if (table[i].score < 0) table[i].score = 0;
		table[i].name[31] = 0;
	};
	
	// see if we should be added to the table
	int highlight = -1;
	if (score >= 0)
	{
		for (i=0; i<10; i++)
		{
			if (table[i].score < score) break;
		};
		
		if (i != 10)
		{
			// someone scored less than us!
			char *playerName = gwmGetInput(NULL, "High score", "Please enter your name:", pwd->pw_name);
			if (playerName != NULL)
			{
				if (strlen(playerName) > 31) playerName[31] = 0;
				memmove(&table[i+1], &table[i], sizeof(HighScoreEntry)*(9-i));
				strcpy(table[i].name, playerName);
				table[i].score = score;
				
				// now save it
				fp = fopen(tablePath, "wb");
				if (fp == NULL)
				{
					gwmMessageBox(NULL, "High score", "Could not write to the high score file!",
							GWM_MBICON_ERROR | GWM_MBUT_OK);
				}
				else
				{
					fwrite(table, sizeof(HighScoreEntry), 10, fp);
					fclose(fp);
				};
				
				highlight = i;
			};
		};
	};
	
	free(tablePath);
	
	// display the table
	GWMLabel* lblNames[10];
	GWMLabel* lblScores[10];
	GWMLayout *mainBox;
	GWMLayout *scoreTable;
	GWMLayout *btnBox;
	GWMButton *btnClose;
	
	GWMWindow *dialog = gwmCreateModal("High score", 0, 0, 0, 0);
	mainBox = gwmCreateBoxLayout(GWM_BOX_VERTICAL);
	gwmSetWindowLayout(dialog, mainBox);
	
	scoreTable = gwmCreateFlexLayout(2);
	gwmBoxLayoutAddLayout(mainBox, scoreTable, 0, 0, GWM_BOX_FILL);
	
	for (i=0; i<10; i++)
	{
		GWMLabel *lblName = gwmNewLabel(dialog);
		gwmFlexLayoutAddWindow(scoreTable, lblName, 0, 0, GWM_FLEX_FILL, GWM_FLEX_CENTER);
		lblNames[i] = lblName;
		
		GWMLabel *lblScore = gwmNewLabel(dialog);
		gwmFlexLayoutAddWindow(scoreTable, lblScore, 0, 0, GWM_FLEX_FILL, GWM_FLEX_CENTER);
		lblScores[i] = lblScore;
		
		char *text;
		asprintf(&text, "%d. %s    ", i+1, table[i].name);
		gwmSetLabelText(lblName, text);
		free(text);
		
		asprintf(&text, "%d", table[i].score);
		gwmSetLabelText(lblScore, text);
		free(text);
		
		if (i == highlight)
		{
			gwmSetLabelFont(lblName, GWM_FONT_STRONG);
			gwmSetLabelFont(lblScore, GWM_FONT_STRONG);
		};
	};
	
	btnBox = gwmCreateBoxLayout(GWM_BOX_HORIZONTAL);
	gwmBoxLayoutAddLayout(mainBox, btnBox, 0, 0, GWM_BOX_FILL);
	
	gwmBoxLayoutAddSpacer(btnBox, 1, 0, 0);
	
	btnClose = gwmCreateStockButton(dialog, GWM_SYM_CLOSE);
	gwmBoxLayoutAddWindow(btnBox, btnClose, 0, 5, GWM_BOX_ALL);
	
	gwmFit(dialog);
	gwmFocus(btnClose);
	gwmRunModal(dialog, GWM_WINDOW_NOSYSMENU | GWM_WINDOW_NOTASKBAR);
	gwmSetWindowFlags(dialog, GWM_WINDOW_HIDDEN);
	
	gwmDestroyButton(btnClose);
	for (i=0; i<10; i++)
	{
		gwmDestroyLabel(lblNames[i]);
		gwmDestroyLabel(lblScores[i]);
	};
	
	gwmDestroyBoxLayout(mainBox);
	gwmDestroyBoxLayout(btnBox);
	gwmDestroyFlexLayout(scoreTable);
	
	gwmDestroyWindow(dialog);
};
