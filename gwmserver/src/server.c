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

#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/un.h>
#include <libddi.h>
#include <libgwm.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <pthread.h>
#include <inttypes.h>
#include <libgwm.h>
#include <stdint.h>

#include "window.h"
#include "screen.h"

typedef struct WndLookup_
{
	struct WndLookup_*		next;
	uint64_t			id;		/* application-chosen window ID */
	Window*				win;
} WndLookup;

/**
 * Store a window handle. Note that this assumes there is no handle currently with that ID!
 * Check with wltGet() first! Also increments the refcount of the window.
 */
void wltPut(WndLookup **wlt, uint64_t id, Window *win)
{
	uint64_t hash = id & 0xFF;
	
	WndLookup *lookup = (WndLookup*) malloc(sizeof(WndLookup));
	lookup->next = wlt[hash];
	lookup->id = id;
	lookup->win = win;
	wndUp(win);
	
	wlt[hash] = lookup;
};

/**
 * Get a window from the list. Returns NULL if no such window exists. Increments the refcount of
 * the returned window!
 */
Window* wltGet(WndLookup **wlt, uint64_t id)
{
	uint64_t hash = id & 0xFF;
	
	WndLookup *lookup;
	for (lookup=wlt[hash]; lookup!=NULL; lookup=lookup->next)
	{
		if (lookup->id == id)
		{
			wndUp(lookup->win);
			return lookup->win;
		};
	};
	
	return NULL;
};

/**
 * Remove a window handle from the WLT and return it.
 */
Window* wltRemove(WndLookup **wlt, uint64_t id)
{
	uint64_t hash = id & 0xFF;
	
	WndLookup *lookup;
	WndLookup *prev = NULL;
	for (lookup=wlt[hash]; lookup!=NULL; lookup=lookup->next)
	{
		if (lookup->id == id)
		{
			if (prev == NULL)
			{
				wlt[hash] = lookup->next;
			}
			else
			{
				prev->next = lookup->next;
			};
			
			Window *win = lookup->win;
			free(lookup);
			return win;
		};
		
		prev = lookup;
	};
	
	return NULL;
};

void* clientThreadFunc(void *context)
{
	int sockfd = (int) (uintptr_t) context;
	
	/**
	 * A hashtable of windows. The hashkey is the bottom 8 bits of a window ID
	 * and it points to a linked list of WndLookup structures with IDs ending
	 * in that hashkey.
	 */
	WndLookup* wlt[256];
	memset(wlt, 0, sizeof(void*) * 256);
	wltPut(wlt, 0, desktopWindow);
	
	while (1)
	{
		GWMCommand cmd;
		
		ssize_t sz = read(sockfd, &cmd, sizeof(GWMCommand));
		if (sz == -1)
		{
			printf("[gwmserver] read from client socket: %s\n", strerror(errno));
			break;
		};
		
		if (sz == 0)
		{
			// client disconnected
			break;
		};
		
		if (sz < sizeof(int))
		{
			// too small to even store a command
			printf("[gwmserver] application sent command structure smaller than command ID (%ld bytes)\n", sz);
			break;
		};
		
		if (cmd.cmd == GWM_CMD_CREATE_WINDOW)
		{
			if (sz < sizeof(cmd.createWindow))
			{
				printf("[gwmserver] GWM_CMD_CREATE_WINDOW command too small\n");
				break;
			};
			
			if (cmd.createWindow.pars.x == GWM_POS_UNSPEC)
			{
				cmd.createWindow.pars.x = 5;
			};
			
			if (cmd.createWindow.pars.y == GWM_POS_UNSPEC)
			{
				cmd.createWindow.pars.y = 5;
			};

			GWMMessage resp;
			resp.createWindowResp.type = GWM_MSG_CREATE_WINDOW_RESP;
			resp.createWindowResp.seq = cmd.createWindow.seq;
			memcpy(&resp.createWindowResp.format, &screen->format, sizeof(DDIPixelFormat));
			resp.createWindowResp.width = cmd.createWindow.pars.width;
			resp.createWindowResp.height = cmd.createWindow.pars.height;

			Window *check = wltGet(wlt, cmd.createWindow.id);
			if (check != NULL)
			{
				wndDown(check);
				resp.createWindowResp.status = GWM_ERR_INVAL;
				write(sockfd, &resp, sizeof(GWMMessage));
			}
			else
			{
				DDISurface *canvas = ddiOpenSurface(cmd.createWindow.surfID);
				if (canvas == NULL)
				{
					resp.createWindowResp.status = GWM_ERR_NOSURF;
					write(sockfd, &resp, sizeof(GWMMessage));
				}
				else
				{
					Window *parent = wltGet(wlt, cmd.createWindow.parent);
					if (parent == NULL)
					{
						resp.createWindowResp.status = GWM_ERR_NOWND;
						write(sockfd, &resp, sizeof(GWMMessage));
					}
					else
					{
						Window *win = wndCreate(parent, &cmd.createWindow.pars, cmd.createWindow.id,
										sockfd, canvas);
						wndDown(parent);
						if (win == NULL)
						{
							resp.createWindowResp.status = GWM_ERR_NOWND;
							write(sockfd, &resp, sizeof(GWMMessage));
						}
						else
						{
							wltPut(wlt, cmd.createWindow.id, win);
							wndDown(win);
							resp.createWindowResp.status = 0;
							write(sockfd, &resp, sizeof(GWMMessage));
							wndDirty(win);
							wndDrawScreen();
						};
					};
				};
			};
		}
		else if (cmd.cmd == GWM_CMD_POST_DIRTY)
		{
			if (sz < sizeof(cmd.postDirty))
			{
				printf("[gwmserver] GWM_CMD_POST_DIRTY command too small\n");
				break;
			};
			
			GWMMessage resp;
			resp.postDirtyResp.type = GWM_MSG_POST_DIRTY_RESP;
			resp.postDirtyResp.seq = cmd.postDirty.seq;
			
			Window *win = wltGet(wlt, cmd.postDirty.id);
			if (win == NULL)
			{
				resp.postDirtyResp.status = GWM_ERR_NOWND;
				write(sockfd, &resp, sizeof(GWMMessage));
			}
			else
			{
				pthread_mutex_lock(&win->lock);
				ddiOverlay(win->canvas, 0, 0, win->front, 0, 0, win->canvas->width, win->canvas->height);
				pthread_mutex_unlock(&win->lock);
				
				resp.postDirtyResp.status = 0;
				write(sockfd, &resp, sizeof(GWMMessage));
				
				wndDirty(win);
				wndDown(win);
				wndDrawScreen();
			};
		}
		else if (cmd.cmd == GWM_CMD_DESTROY_WINDOW)
		{
			if (sz < sizeof(cmd.destroyWindow))
			{
				printf("[gwmserver] GWM_CMD_DESTROY_WINDOW command too small\n");
				break;
			};
			
			Window *win = wltRemove(wlt, cmd.destroyWindow.id);
			wndDestroy(win);	// also decrefs
		}
		else if (cmd.cmd == GWM_CMD_SCREEN_SIZE)
		{
			if (sz < sizeof(cmd.screenSize))
			{
				printf("[gwmserver] GWM_CMD_SCREEN_SIZE command too small\n");
				break;
			};
			
			GWMMessage resp;
			resp.screenSizeResp.type = GWM_MSG_SCREEN_SIZE_RESP;
			resp.screenSizeResp.seq = cmd.screenSize.seq;
			resp.screenSizeResp.width = (int) screen->width;
			resp.screenSizeResp.height = (int) screen->height;
			write(sockfd, &resp, sizeof(GWMMessage));
		}
		else if (cmd.cmd == GWM_CMD_SET_FLAGS)
		{
			if (sz < sizeof(cmd.setFlags))
			{
				printf("[gwmserver] GWM_CMD_SET_FLAGS command too small\n");
				break;
			};
			
			GWMMessage resp;
			resp.setFlagsResp.type = GWM_MSG_SET_FLAGS_RESP;
			resp.setFlagsResp.seq = cmd.setFlags.seq;
			
			Window *wnd = wltGet(wlt, cmd.setFlags.win);
			if (wnd == NULL)
			{
				resp.setFlagsResp.status = GWM_ERR_NOWND;
			}
			else
			{
				wndSetFlags(wnd, cmd.setFlags.flags);
				wndDirty(wnd);
				wndDrawScreen();
				resp.setFlagsResp.status = 0;
			};
			
			write(sockfd, &resp, sizeof(GWMMessage));
		}
		else if (cmd.cmd == GWM_CMD_SET_CURSOR)
		{
			if (sz < sizeof(cmd.setCursor))
			{
				printf("[gwmserver] GWM_CMD_SET_CURSOR command too small\n");
				break;
			};
			
			GWMMessage resp;
			resp.setCursorResp.type = GWM_MSG_SET_CURSOR_RESP;
			resp.setCursorResp.seq = cmd.setCursor.seq;
			
			if ((cmd.setCursor.cursor < 0) || (cmd.setCursor.cursor >= GWM_CURSOR_COUNT))
			{
				resp.setCursorResp.status = GWM_ERR_INVAL;
			}
			else
			{
				Window *win = wltGet(wlt, cmd.setCursor.win);
				if (win == NULL)
				{
					resp.setCursorResp.status = GWM_ERR_NOWND;
				}
				else
				{
					win->cursor = cmd.setCursor.cursor;
					wndDown(win);
					wndDrawScreen();
					resp.setCursorResp.status = 0;
				};
			};
			
			write(sockfd, &resp, sizeof(GWMMessage));
		}
		else if (cmd.cmd == GWM_CMD_SET_ICON)
		{
			if (sz < sizeof(cmd.setIcon))
			{
				printf("[gwmserver] GWM_CMD_SET_ICON command too small\n");
				break;
			};
			
			GWMMessage resp;
			resp.setIconResp.type = GWM_MSG_SET_ICON_RESP;
			resp.setIconResp.seq = cmd.setIcon.seq;
			
			Window *win = wltGet(wlt, cmd.setIcon.win);
			if (win == NULL)
			{
				resp.setIconResp.status = GWM_ERR_NOWND;
			}
			else
			{
				resp.setIconResp.status = wndSetIcon(win, cmd.setIcon.surfID);
				wndDown(win);
			};
			
			write(sockfd, &resp, sizeof(GWMMessage));
		}
		else if (cmd.cmd == GWM_CMD_GET_FORMAT)
		{
			if (sz < sizeof(cmd.getFormat))
			{
				printf("[gwmserver] GWM_CMD_GET_FORMAT command too small\n");
				break;
			};
			
			GWMMessage resp;
			resp.getFormatResp.type = GWM_MSG_GET_FORMAT_RESP;
			resp.getFormatResp.seq = cmd.getFormat.seq;
			memcpy(&resp.getFormatResp.format, &screen->format, sizeof(DDIPixelFormat));
			write(sockfd, &resp, sizeof(GWMMessage));
		}
		else if (cmd.cmd == GWM_CMD_REL_TO_ABS)
		{
			if (sz < sizeof(cmd.relToAbs))
			{
				printf("[gwmserver] GWM_CMD_REL_TO_ABS command too small\n");
				break;
			};
			
			int absX = 0, absY = 0;
			Window *win = wltGet(wlt, cmd.relToAbs.win);
			if (win != NULL)
			{
				wndRelToAbs(win, cmd.relToAbs.relX, cmd.relToAbs.relY, &absX, &absY);
				wndDown(win);
			};
			
			GWMMessage resp;
			resp.relToAbsResp.type = GWM_MSG_REL_TO_ABS_RESP;
			resp.relToAbsResp.seq = cmd.relToAbs.seq;
			resp.relToAbsResp.absX = absX;
			resp.relToAbsResp.absY = absY;
			write(sockfd, &resp, sizeof(GWMMessage));
		}
		else
		{
			printf("[gwmserver] received unknown command, %ld bytes (cmd=%d)\n", sz, cmd.cmd);
		};
	};
	
	close(sockfd);
	
	// destory any windows belonging to this application
	int i;
	for (i=0; i<256; i++)
	{
		while (wlt[i] != NULL)
		{
			WndLookup *lookup = wlt[i];
			wlt[i] = lookup->next;
			wndDestroy(lookup->win);
			free(lookup);
		};
	};
	
	return NULL;
};

void runServer(int sockfd)
{
	struct sockaddr_storage caddr;
	while (1)
	{
		socklen_t addrlen = sizeof(struct sockaddr_storage);
		int client = accept(sockfd, (struct sockaddr*) &caddr, &addrlen);
		if (client != -1)
		{
			pthread_t thread;
			if (pthread_create(&thread, NULL, clientThreadFunc, (void*) (uintptr_t) client) == 0)
			{
				pthread_detach(thread);
			}
			else
			{
				// TODO: error reporting
				close(client);
			};
		};
	};
};
