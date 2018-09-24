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

#ifndef FILEMGR_H_
#define FILEMGR_H_

#include <libgwm.h>

extern GWMWindow *topWindow;
extern GWMMenu *menuEdit;
extern DDIFont *fntCategory;
extern int desktopMode;

/**
 * File manager events.
 */
enum
{
	FILEMGR_EVENT_AGG_ = GWM_EVENT_AGG,
	
	/**
	 * DirView aggregating events.
	 */
	DV_EVENT_CHECK,
	
	FILEMGR_EVENT_NONCASCADING_ = GWM_EVENT_USER,
	
	/**
	 * Task events.
	 */
	TASK_EVENT_MSGBOX,
	
	FILEMGR_EVENT_CASCADING_ = (GWM_EVENT_CASCADING | GWM_EVENT_USER),
	
	/**
	 * DirView events.
	 */
	DV_EVENT_CHDIR,
};

/**
 * File manager symbols.
 */
enum
{
	FILEMGR_SYM_ = GWM_SYM_USER,
	
	/**
	 * DirView-related commands.
	 */
	DV_SYM_MKDIR,
	DV_SYM_TERMINAL,
	DV_SYM_PROPS,
	
	/**
	 * Properties dialog "chmod" commands.
	 */
	PROP_CHMOD,
	PROP_CHMOD_END = PROP_CHMOD + 12,
};

#endif
