/*
	Glidix kernel

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

#include <glidix/audio.h>
#include <glidix/memory.h>
#include <glidix/sched.h>
#include <glidix/string.h>

//static AudioAppBuffer* 	firstBuffer;
static AudioOutput* 	currentOut;
static AudioOutput*	firstOut;
static Semaphore	semOutputList;
static Semaphore	semSamplesToMix;

AudioOutput* audioCreateOutput(AudioStream* audioStream)
{
	AudioOutput *device = (AudioOutput*) NEW(AudioOutput);
	device->audioStream = audioStream;
	device->prev = NULL;
	
	semWait(&semOutputList);
	if (firstOut != NULL) firstOut->prev = device;
	device->next = firstOut;
	firstOut = device;
	semSignal(&semOutputList);
	
	return device;
};

void audioDeleteOutput(AudioOutput* device)
{
	semWait(&semOutputList);
	if(device == firstOut) firstOut = device->next;
	if(device->next != NULL) device->next->prev = device->prev;
	if(device->prev != NULL) device->prev->next = device->next;
	semSignal(&semOutputList);
	
	kfree(device);
};

void audioMixerThread()
{
	while(1)
	{
		int samplesToMix = semWaitGen(&semSamplesToMix, 100, 0, 0);
		semWait(&semOutputList);
		while(samplesToMix--)
		{
			if(currentOut != NULL)
			{
				
				if(currentOut->audioStream->bitsPSample == 8)
				{
					uint8_t *ptr = (uint8_t*) currentOut->audioStream->buffer + currentOut->audioStream->head;
					if(currentOut->audioStream->head%2) ptr[currentOut->audioStream->head] = 0;
					else ptr[currentOut->audioStream->head] = 0xFF;
				}
				if(currentOut->audioStream->bitsPSample == 16)
				{
					uint16_t *ptr = (uint16_t*) currentOut->audioStream->buffer + currentOut->audioStream->head;
					if(currentOut->audioStream->head%2) ptr[currentOut->audioStream->head] = 0;
					else ptr[currentOut->audioStream->head] = 0xFFFF;
				}
				currentOut->audioStream->head = (currentOut->audioStream->head + 1) % currentOut->audioStream->nOfSamples;
			}
		}
		semSignal(&semOutputList);
	}
};

void audioInit()
{
	semInit(&semOutputList);
	semInit2(&semSamplesToMix, 0);
	
	KernelThreadParams pars;
	memset(&pars, 0, sizeof(KernelThreadParams));
	pars.name = "Audio Mixer Thread";
	pars.stackSize = DEFAULT_STACK_SIZE;
	CreateKernelThread(audioMixerThread, &pars, NULL);
};