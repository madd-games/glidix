/*
	Glidix Runtime

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

#include <stdlib.h>
#include <string.h>

void qsort(void *base_, size_t nmemb, size_t size, int (*compar)(const void *, const void *))
{
	char *base = (char*) base_;
	size_t i;

	// there is no point of sorting less than 2 members (as it is implicitly already sorted)
	if (nmemb < 2)
	{
		return;
	};

	int doneSwaps = 0;
	char *swapBuffer = (char*) malloc(size);

	for (i=0; i<(nmemb-1); i++)
	{
		char *a = &base[i*size];
		char *b = &a[size];

		if (compar(a, b) > 0)			// a > b
		{
			doneSwaps = 1;
			memcpy(swapBuffer, a, size);
			memcpy(a, b, size);
			memcpy(b, swapBuffer, size);
		};
	};

	if (doneSwaps)
	{
		// we have changed stuff in the list, and the largest element is now on
		// the end, so sort the rest.
		qsort(base_, nmemb-1, size, compar);
	};
};
