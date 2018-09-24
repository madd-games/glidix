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

#include "task.h"
#include "filemgr.h"

static int taskHandler(GWMEvent *ev, GWMWindow *win, void *context)
{
	TaskData *data = (TaskData*) context;
	
	switch (ev->type)
	{
	case GWM_EVENT_UPDATE:
		if (data->change == TASK_CHANGE_TEXT)
		{
			gwmSetLabelText(data->label, data->text);
		}
		else if (data->change == TASK_CHANGE_PROGRESS)
		{
			gwmSetScaleValue(data->progbar, data->prog);
		};
		data->change = 0;
		sem_post(&data->sem);
		
		if (data->done)
		{
			return GWM_EVSTATUS_BREAK;
		};
		
		return GWM_EVSTATUS_OK;
	case TASK_EVENT_MSGBOX:
		data->msgAnswer = gwmMessageBox(win, data->msgCaption, data->msgText, data->msgFlags);
		sem_post(&data->sem);
		return GWM_EVSTATUS_OK;
	default:
		return GWM_EVSTATUS_CONT;
	};
};

static void* taskThread(void *context)
{
	TaskData *data = (TaskData*) context;
	data->taskfunc(data, data->context);
	data->done = 1;
	gwmPostUpdate(data->win);
	return NULL;
};

void taskRun(const char *caption, void (*taskfunc)(TaskData*, void *), void *context)
{
	TaskData data;
	data.done = 0;
	data.context = context;
	data.taskfunc = taskfunc;
	data.text = "";
	data.prog = 0;
	sem_init(&data.sem, 0, 0);
	data.change = 0;
	
	GWMWindow *win = gwmCreateModal(caption, 0, 0, 0, 0);
	gwmPushEventHandler(win, taskHandler, &data);
	data.win = win;
	
	GWMLayout *box = gwmCreateBoxLayout(GWM_BOX_VERTICAL);
	gwmSetWindowLayout(win, box);
	
	GWMLabel *label = gwmNewLabel(win);
	gwmSetLabelText(label, "...");
	data.label = label;
	gwmBoxLayoutAddWindow(box, label, 0, 5, GWM_BOX_ALL | GWM_BOX_FILL);
	
	GWMProgressBar *progbar = gwmNewProgressBar(win);
	data.progbar = progbar;
	gwmBoxLayoutAddWindow(box, progbar, 0, 5, GWM_BOX_LEFT | GWM_BOX_RIGHT | GWM_BOX_DOWN | GWM_BOX_FILL);
	
	gwmLayout(win, 250, 0);
	
	if (pthread_create(&data.thread, NULL, taskThread, &data) != 0)
	{
		abort();
	};
	
	gwmRunModal(win, GWM_WINDOW_MKFOCUSED | GWM_WINDOW_NOSYSMENU | GWM_WINDOW_NOTASKBAR);
	gwmSetWindowFlags(win, GWM_WINDOW_HIDDEN);
	
	gwmDestroyProgressBar(progbar);
	gwmDestroyLabel(label);
	
	gwmDestroyBoxLayout(box);
	
	gwmDestroyWindow(win);
};

void taskSetText(TaskData *data, const char *text)
{
	data->text = text;
	data->change = TASK_CHANGE_TEXT;
	gwmPostUpdate(data->win);
	sem_wait(&data->sem);
};

void taskSetProgress(TaskData *data, float prog)
{
	data->prog = prog;
	data->change = TASK_CHANGE_PROGRESS;
	gwmPostUpdate(data->win);
	sem_wait(&data->sem);
};

int taskMessageBox(TaskData *data, const char *caption, const char *text, int flags)
{
	data->msgCaption = caption;
	data->msgText = text;
	data->msgFlags = flags;
	gwmPostUpdateEx(data->win, TASK_EVENT_MSGBOX, 0);
	sem_wait(&data->sem);
	return data->msgAnswer;
};
