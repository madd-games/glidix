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

#define MAX(x, y) (((x) > (y)) ? (x) : (y))
#define MIN(x, y) (((x) < (y)) ? (x) : (y))

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
		
		// NOTE: Please keep the following in numeric order of commands as given in libgwm.h
		if (cmd.cmd == GWM_CMD_CREATE_WINDOW)
		{
			if (sz < sizeof(cmd.createWindow))
			{
				printf("[gwmserver] GWM_CMD_CREATE_WINDOW command too small\n");
				break;
			};
			
			if (cmd.createWindow.pars.x == GWM_POS_UNSPEC)
			{
				cmd.createWindow.pars.x = (screen->width - cmd.createWindow.pars.width)/2;
			};
			
			if (cmd.createWindow.pars.y == GWM_POS_UNSPEC)
			{
				cmd.createWindow.pars.y = (screen->height - cmd.createWindow.pars.height)/2;
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
		else if (cmd.cmd == GWM_CMD_RETHEME)
		{
			pthread_mutex_lock(&desktopWindow->lock);
			Window *win = desktopWindow->children;
			if (win != NULL) wndUp(win);
			pthread_mutex_unlock(&desktopWindow->lock);
			
			while (win != NULL)
			{
				if (win->isDecoration)
				{
					wndDecorate(win, win->children);
				};
				
				pthread_mutex_lock(&desktopWindow->lock);
				Window *next = win->next;
				if (next != NULL) wndUp(next);
				wndDown(win);
				win = next;
				pthread_mutex_unlock(&desktopWindow->lock);
			};
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
		else if (cmd.cmd == GWM_CMD_GET_WINDOW_LIST)
		{
			if (sz < sizeof(cmd.getWindowList))
			{
				printf("[gwmserver] GWM_CMD_GET_WINDOW_LIST command too small\n");
				break;
			};
			
			GWMMessage resp;
			resp.getWindowListResp.type = GWM_MSG_GET_WINDOW_LIST_RESP;
			resp.getWindowListResp.seq = cmd.getWindowList.seq;
			wndGetWindowList(&resp.getWindowListResp.focused, resp.getWindowListResp.wins, &resp.getWindowListResp.count);
			write(sockfd, &resp, sizeof(GWMMessage));
		}
		else if (cmd.cmd == GWM_CMD_GET_WINDOW_PARAMS)
		{
			if (sz < sizeof(cmd.getWindowParams))
			{
				printf("[gwmserver] GWM_CMD_GET_WINDOW_PARAMS command too small\n");
				break;
			};
			
			GWMMessage resp;
			resp.getWindowParamsResp.type = GWM_MSG_GET_WINDOW_PARAMS_RESP;
			resp.getWindowParamsResp.seq = cmd.getWindowParams.seq;
			resp.getWindowParamsResp.status = wndGetWindowParams(&cmd.getWindowParams.ref, &resp.getWindowParamsResp.params);
			write(sockfd, &resp, sizeof(GWMMessage));
		}
		else if (cmd.cmd == GWM_CMD_SET_LISTEN_WINDOW)
		{
			if (sz < sizeof(cmd.setListenWindow))
			{
				printf("[gwmserver] GWM_CMD_SET_LISTEN_WINDOW command too small\n");
				break;
			};
			
			Window *win = wltGet(wlt, cmd.setListenWindow.win);
			if (win != NULL)
			{
				wndSetListenWindow(win);
				wndDown(win);
			};
		}
		else if (cmd.cmd == GWM_CMD_TOGGLE_WINDOW)
		{
			if (sz < sizeof(cmd.toggleWindow))
			{
				printf("[gwmserver] GWM_CMD_TOGGLE_WINDOW command too small\n");
				break;
			};
			
			GWMMessage resp;
			resp.toggleWindowResp.type = GWM_MSG_TOGGLE_WINDOW_RESP;
			resp.toggleWindowResp.seq = cmd.toggleWindow.seq;
			resp.toggleWindowResp.status = wndToggle(&cmd.toggleWindow.ref);
			write(sockfd, &resp, sizeof(GWMMessage));
		}
		else if (cmd.cmd == GWM_CMD_GET_GLOB_REF)
		{
			if (sz < sizeof(cmd.getGlobRef))
			{
				printf("[gwmserver] GWM_CMD_GET_GLOB_REF command too small\n");
				break;
			};
			
			GWMMessage resp;
			resp.getGlobRefResp.type = GWM_MSG_GET_GLOB_REF_RESP;
			resp.getGlobRefResp.seq = cmd.getGlobRef.seq;
			resp.getGlobRefResp.ref.id = cmd.getGlobRef.win;
			resp.getGlobRefResp.ref.fd = sockfd;
			write(sockfd, &resp, sizeof(GWMMessage));
		}
		else if (cmd.cmd == GWM_CMD_ATOMIC_CONFIG)
		{
			if (sz < sizeof(cmd.atomicConfig))
			{
				printf("[gwmserver] GWM_CMD_ATOMIC_CONFIG command too small\n");
				break;
			};
			
			Window *win = wltGet(wlt, cmd.atomicConfig.win);
			if (win != NULL)
			{
				int which = cmd.atomicConfig.which;
				int oldX, oldY, newX, newY;
				int shouldInvalidate = 1;
				int oldEndX, oldEndY, newEndX, newEndY;
				
				if (win->decorated)
				{
					pthread_mutex_lock(&win->parent->lock);
					
					oldX = win->parent->params.x;
					oldY = win->parent->params.y;
					
					if (which & GWM_AC_X)
					{
						win->parent->params.x = newX = cmd.atomicConfig.x;
					}
					else
					{
						newX = oldX;
					};
					
					if (which & GWM_AC_Y)
					{
						win->parent->params.y = newY = cmd.atomicConfig.y;
					}
					else
					{
						newY = oldY;
					};
					
					pthread_mutex_unlock(&win->parent->lock);
				}
				else
				{
					shouldInvalidate = !wndRelToAbs(win, 0, 0, &oldX, &oldY);
					pthread_mutex_lock(&win->lock);
					
					if (which & GWM_AC_X) win->params.x = cmd.atomicConfig.x;
					if (which & GWM_AC_Y) win->params.y = cmd.atomicConfig.y;
					
					pthread_mutex_unlock(&win->lock);
					wndRelToAbs(win, 0, 0, &newX, &newY);
				};
				
				// changing of width, height and canvas is independent of decoration
				// scroll is independent too of course
				pthread_mutex_lock(&win->lock);
				
				oldEndX = oldX + win->params.width;
				oldEndY = oldY + win->params.height;
				
				if (win->decorated)
				{
					oldEndX = oldX + win->parent->params.width;
					oldEndY = oldY + win->parent->params.height;
				};
				
				if (which & GWM_AC_WIDTH) win->params.width = cmd.atomicConfig.width;
				if (which & GWM_AC_HEIGHT) win->params.height = cmd.atomicConfig.height;
				
				if (which & GWM_AC_CAPTION)
				{
					memcpy(win->params.caption, cmd.atomicConfig.caption, 256);
				};
				
				newEndX = newX + win->params.width;
				newEndY = newY + win->params.height;
				
				if (win->decorated)
				{
					newEndX = newX + win->parent->params.width;
					newEndY = newY + win->parent->params.height;
				};
				
				if (which & GWM_AC_SCROLL_X) win->scrollX = cmd.atomicConfig.scrollX;
				if (which & GWM_AC_SCROLL_Y) win->scrollY = cmd.atomicConfig.scrollY;
				
				if (which & GWM_AC_CANVAS)
				{
					DDISurface *newCanvas = ddiOpenSurface(cmd.atomicConfig.canvasID);
					if (newCanvas != NULL)
					{
						ddiDeleteSurface(win->front);
						ddiDeleteSurface(win->canvas);
						win->canvas = newCanvas;
						win->front = ddiCreateSurface(&screen->format, win->canvas->width,
										win->canvas->height, (char*)newCanvas->data,
										0);
					};
				};
				
				pthread_mutex_unlock(&win->lock);
				
				// update the decoration if needed
				if (win->decorated)
				{
					if (which & (GWM_AC_WIDTH | GWM_AC_HEIGHT | GWM_AC_CAPTION))
					{
						pthread_mutex_lock(&win->parent->lock);
						
						oldEndX = oldX + win->parent->params.width;
						oldEndY = oldY + win->parent->params.height;
						
						if (which & GWM_AC_WIDTH)
							win->parent->params.width = cmd.atomicConfig.width + 2 * WINDOW_BORDER_WIDTH;
						if (which & GWM_AC_HEIGHT)
							win->parent->params.height = cmd.atomicConfig.height + WINDOW_CAPTION_HEIGHT + WINDOW_BORDER_WIDTH;
						
						newEndX = newX + win->parent->params.width;
						newEndY = newY + win->parent->params.height;
						
						ddiDeleteSurface(win->parent->canvas);
						ddiDeleteSurface(win->parent->front);
						
						win->parent->canvas = ddiCreateSurface(&screen->format, win->parent->params.width, win->parent->params.height, NULL, 0);
						win->parent->front = ddiCreateSurface(&screen->format, win->parent->params.width, win->parent->params.height, NULL, 0);
						pthread_mutex_unlock(&win->parent->lock);
						
						wndDecorate(win->parent, win);
					};
				};
				
				if (shouldInvalidate)
				{
					int invX = MIN(oldX, newX);
					int invY = MIN(oldY, newY);
					int endX = MAX(oldEndX, newEndX);
					int endY = MAX(oldEndY, newEndY);
					
					wndInvalidate(invX, invY, endX - invX, endY - invY);
				};

				wndDown(win);
				wndDrawScreen();
			};
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
		else if (cmd.cmd == GWM_CMD_REDRAW_SCREEN)
		{
			wndInvalidate(0, 0, screen->width, screen->height);
		}
		else if (cmd.cmd == GWM_CMD_SCREENSHOT_WINDOW)
		{
			if (sz < sizeof(cmd.screenshotWindow))
			{
				printf("[gwmserver] GWM_CMD_SCREENSHOT_WINDOW command too small\n");
				break;
			};
			
			GWMMessage resp;
			resp.screenshotWindowResp.type = GWM_MSG_SCREENSHOT_WINDOW_RESP;
			resp.screenshotWindowResp.seq = cmd.screenshotWindow.seq;
			
			DDISurface *target = ddiOpenSurface(cmd.screenshotWindow.surfID);
			if (target == NULL)
			{
				resp.screenshotWindowResp.status = GWM_ERR_NOSURF;
			}
			else
			{
				resp.screenshotWindowResp.status =
					wndScreenshot(target, &cmd.screenshotWindow.ref,
						&resp.screenshotWindowResp.width, &resp.screenshotWindowResp.height);
				ddiDeleteSurface(target);
			};
			
			write(sockfd, &resp, sizeof(GWMMessage));
		}
		else if (cmd.cmd == GWM_CMD_GET_GLOB_ICON)
		{
			if (sz < sizeof(cmd.getGlobIcon))
			{
				printf("[gwmserver] GWM_CMD_GET_GLOB_ICON command too small\n");
				break;
			};
			
			GWMMessage resp;
			resp.getGlobIconResp.type = GWM_MSG_GET_GLOB_ICON_RESP;
			resp.getGlobIconResp.seq = cmd.getGlobIcon.seq;
			resp.getGlobIconResp.status = wndGetGlobIcon(&cmd.getGlobIcon.ref, &resp.getGlobIconResp.surfID);
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
		}
		else
		{
			perror("[gwmserver] accept\n");
		};
	};
};
