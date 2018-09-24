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

#ifndef TASK_H_
#define TASK_H_

#include <libgwm.h>
#include <pthread.h>
#include <semaphore.h>

/**
 * Task dialog. This is a special modal dialog which executes a separate thread, which can then
 * do its job, and it updates the progress on the dialog.
 */

#define	TASK_CHANGE_TEXT				1
#define	TASK_CHANGE_PROGRESS				2

typedef struct TaskData_
{
	/**
	 * The thread doing the work.
	 */
	pthread_t					thread;
	
	/**
	 * Initially 0, set to 1 by the thread once the task has been completed.
	 */
	int						done;
	
	/**
	 * Task-specific data.
	 */
	void*						context;
	
	/**
	 * Task callback.
	 */
	void (*taskfunc)(struct TaskData_*, void *);
	
	/**
	 * The label.
	 */
	GWMLabel*					label;
	
	/**
	 * The progress bar.
	 */
	GWMProgressBar*					progbar;
	
	/**
	 * Main window.
	 */
	GWMWindow*					win;
	
	/**
	 * Current label.
	 */
	const char*					text;
	
	/**
	 * Current progress (0-1).
	 */
	float						prog;
	
	/**
	 * A semaphore which is signalled every update event, to synchronise accesses to 'text' and 'prog'.
	 */
	sem_t						sem;
	
	/**
	 * Which thing should be changed on update (text or progress).
	 */
	int						change;
	
	/**
	 * Parameters and answer for message box.
	 */
	const char*					msgCaption;
	const char*					msgText;
	int						msgFlags;
	int						msgAnswer;
} TaskData;

/**
 * Execute a task.
 */
void taskRun(const char *caption, void (*taskfunc)(TaskData *data, void *context), void *context);

/**
 * Change the text on the dialog. This function is meant to be called by the task thread.
 */
void taskSetText(TaskData *data, const char *text);

/**
 * Change the progess on the dialog. This function is meant to be called by the task thread.
 */
void taskSetProgress(TaskData *data, float prog);

/**
 * Safely display a message box and return the answer.
 */
int taskMessageBox(TaskData *data, const char *caption, const char *text, int flags);

#endif
