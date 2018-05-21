/*
	Glidix Package Manager

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

#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

#include "libgpm.h"

static int verbose = 0;

typedef struct
{
	const uint8_t *scan;
	size_t size;
	uint8_t mask;
} BitStreamReader;

typedef struct HuffNode_ HuffNode;
struct HuffNode_
{
	/**
	 * Sub-nodes.
	 */
	HuffNode*		children[2];
	
	/**
	 * Parent node.
	 */
	HuffNode*		parent;
	
	/**
	 * Frequency of this node.
	 */
	uint64_t		freq;
	
	/**
	 * Byte value to which this node decodes; if 256, then this node means "end of data".
	 * Only valid if both children are NULL.
	 */
	int			value;
};

typedef struct MinHeap_ MinHeap;
struct MinHeap_
{
	/**
	 * Links.
	 */
	MinHeap*		prev;
	MinHeap*		next;
	
	/**
	 * Node at this location.
	 */
	HuffNode*		node;
};

int nextbit(BitStreamReader *bsr)
{
	if (bsr->mask == 0)
	{
		if (bsr->size == 0) return -1;
		bsr->scan++;
		bsr->size--;
		bsr->mask = 0x80;
	};
	
	int bit = !!((*bsr->scan) & bsr->mask);
	bsr->mask >>= 1;
	return bit;
};

void addToHeap(MinHeap **heapPtr, HuffNode *node)
{
	MinHeap *new = (MinHeap*) malloc(sizeof(MinHeap));
	new->node = node;
	
	if ((*heapPtr) == NULL)
	{
		new->prev = new->next = NULL;
		(*heapPtr) = new;
		return;
	};
	
	if ((*heapPtr)->node->freq >= node->freq)
	{
		new->next = (*heapPtr);
		(*heapPtr)->prev = new;
		new->prev = NULL;
		(*heapPtr) = new;
		return;
	};
	
	MinHeap *scan = (*heapPtr);
	while (scan->node->freq < node->freq)
	{
		if (scan->next == NULL)
		{
			scan->next = new;
			new->prev = scan;
			new->next = NULL;
			return;
		};
		
		scan = scan->next;
	};
	
	new->next = scan;
	new->prev = scan->prev;
	scan->prev = new;
	new->prev->next = new;
};

static void deleteTree(HuffNode *node)
{
	if (node->children[0] != NULL) deleteTree(node->children[0]);
	if (node->children[1] != NULL) deleteTree(node->children[1]);
	free(node);
};

static size_t hlzDecode(const uint8_t *buffer, size_t compsize, uint8_t *outbuf)
{
	size_t outsize = 0;
	
	MinHeap* heap;
	HuffNode* huffTree;
	
	if (verbose) printf("  Huffman Coding over LZ77\n");
	
	uint8_t bitspec = buffer[0];
	size_t lookbackBits = (bitspec >> 4);
	size_t sizeBits = (bitspec & 0x0F);
	
	if (verbose) printf("  Using %lu lookback bits, %lu size bits\n", lookbackBits, sizeBits);
	
	uint8_t r = buffer[1] & 0x0F;
	if (verbose) printf("  Frequency table encoded with R=%hhu:\n", r);
	
	uint16_t freqTable[257];
	BitStreamReader bsr;
	bsr.scan = &buffer[2];
	bsr.size = compsize - 2;
	bsr.mask = 0x80;
	
	int i;
	for (i=0; i<257; i++)
	{
		uint16_t q=0, m=0;
		while (nextbit(&bsr) == 1) q++;
		
		size_t count = r;
		while (count--)
		{
			m = (m << 1) | nextbit(&bsr);
		};
		
		uint16_t value = (q << r) | m;
		if (verbose)
		{
			printf("    Symbol [%d] appears %hu times\n", i, value);
		};
		
		freqTable[i] = value;
	};
	
	// prepare the heap
	heap = (MinHeap*) malloc(sizeof(MinHeap));
	heap->prev = heap->next = NULL;
	heap->node = (HuffNode*) malloc(sizeof(HuffNode));
	heap->node->children[0] = heap->node->children[1] = NULL;
	heap->node->parent = NULL;
	heap->node->freq = 1;
	heap->node->value = 257;
	
	for (i=0; i<257; i++)
	{
		HuffNode *node = (HuffNode*) malloc(sizeof(HuffNode));
		node->children[0] = node->children[1] = NULL;
		node->parent = NULL;
		node->freq = freqTable[i];
		node->value = i;
	
		addToHeap(&heap, node);
	};
	
	// print them out
	if (verbose)
	{
		printf("  Sorted symbols:\n");
		MinHeap *hp;
		for (hp=heap; hp!=NULL; hp=hp->next)
		{
			printf("    Symbol %d with frequency %lu\n", hp->node->value, hp->node->freq);
		};
	};

	// generate the tree
	while (heap->next != NULL)
	{
		MinHeap *ha = heap;
		MinHeap *hb = ha->next;
		heap = hb->next;
		if (heap != NULL) heap->prev = NULL;
		
		HuffNode *a = ha->node;
		HuffNode *b = hb->node;
		
		free(ha);
		free(hb);
		
		HuffNode *node = (HuffNode*) malloc(sizeof(HuffNode));
		a->parent = b->parent = node;
		node->children[0] = a;
		node->children[1] = b;
		node->parent = NULL;
		node->freq = a->freq + b->freq;
		
		addToHeap(&heap, node);
	};
	
	huffTree = heap->node;
	free(heap);
	
	// decode
	HuffNode *current = huffTree;
	
	while (1)
	{
		int bit = nextbit(&bsr);
		if (verbose)
		{
			printf("%d", bit);
			fflush(stdout);
		};
		current = current->children[bit];
		
		if (current->children[0] == NULL)
		{
			if (verbose) printf("\nSYMBOL %d\n", current->value);
			if (current->value == 257) break;
			else if (current->value == 256)
			{
				uint16_t lookback = 0;
				uint16_t looksize = 0;
				
				size_t count = lookbackBits;
				while (count--)
				{
					lookback = (lookback << 1) | nextbit(&bsr);
				};
				
				count = sizeBits;
				while (count--)
				{
					looksize = (looksize << 1) | nextbit(&bsr);
				};
				
				if (verbose) printf("\nREPEAT <-%hu,%hu>\n", lookback, looksize);
				
				memcpy(outbuf, &outbuf[outsize-lookback-looksize], looksize);
				outsize += looksize;
			}
			else
			{
				outbuf[outsize++] = (uint8_t) current->value;
			};
			
			current = huffTree;
		};
	};

	deleteTree(huffTree);
	return outsize;
};

GPMDecoder* gpmCreateDecoder(const char *filename, int *error)
{
	int fd = open(filename, O_RDONLY | O_CLOEXEC);
	if (fd == -1)
	{
		if (error != NULL) *error = GPM_ERR_OPENFILE;
		return NULL;
	};
	
	GPMDecoder *strm = (GPMDecoder*) malloc(sizeof(GPMDecoder));
	if (strm == NULL)
	{
		close(fd);
		if (error != NULL) *error = GPM_ERR_ALLOC;
		return NULL;
	};
	
	struct stat st;
	fstat(fd, &st);
	
	strm->blockSize = 0;
	strm->blockPos = 0;
	strm->fd = fd;
	strm->fileSize = st.st_size;
	
	MIPHeader header;
	if (read(fd, &header, 4) != 4)
	{
		close(fd);
		free(strm);
		if (error != NULL) *error = GPM_ERR_NOTMIP;
		return NULL;
	};
	
	if (memcmp(header.mhMagic, "MIP", 3) != 0)
	{
		close(fd);
		free(strm);
		if (error != NULL) *error = GPM_ERR_NOTMIP;
		return NULL;
	};
	
	if (header.mhFlags != 0x80)
	{
		close(fd);
		free(strm);
		if (error != NULL) *error = GPM_ERR_SUPPORT;
		return NULL;
	};
	
	return strm;
};

static void nextBlock(GPMDecoder *strm)
{
	uint8_t type;
	if (read(strm->fd, &type, 1) != 1) return;
	
	uint64_t codesize = 0;
	uint8_t c;
	
	do
	{
		if (read(strm->fd, &c, 1) != 1) return;
		codesize = (codesize << 7) | (c & 0x7F);
	} while (c & 0x80);
	
	uint8_t *compbuf = (uint8_t*) malloc(codesize);
	if (compbuf == NULL) return;
	
	read(strm->fd, compbuf, codesize);
	
	if (type == 0x00)
	{
		strm->blockSize = codesize;
		memcpy(strm->block, compbuf, codesize);
	}
	else if (type == 0x01)
	{
		strm->blockSize = hlzDecode(compbuf, codesize, strm->block);
	};
	
	strm->blockPos = 0;
	free(compbuf);
};

size_t gpmRead(GPMDecoder *strm, void *buffer, size_t size)
{
	uint8_t *put = (uint8_t*) buffer;
	size_t outsize = 0;
	while (size)
	{
		if (strm->blockSize == strm->blockPos)
		{
			nextBlock(strm);
		};
		
		if (strm->blockSize == strm->blockPos)
		{
			break;
		};
		
		size_t sizeNow = strm->blockSize - strm->blockPos;
		if (sizeNow > size) sizeNow = size;
		
		memcpy(put, &strm->block[strm->blockPos], sizeNow);
		strm->blockPos += sizeNow;
		size -= sizeNow;
		put += sizeNow;
		outsize += sizeNow;
	};
	
	return outsize;
};

void gpmDestroyDecoder(GPMDecoder *decoder)
{
	close(decoder->fd);
	free(decoder);
};

float gpmGetDecoderProgress(GPMDecoder *decoder)
{
	return (float) lseek(decoder->fd, 0, SEEK_CUR) / (float) decoder->fileSize;
};
