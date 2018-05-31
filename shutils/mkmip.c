/*
	Glidix Shell Utilities

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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <inttypes.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <time.h>
#include <errno.h>

#define	BLOCK_SIZE				(32*1024)

#define	MIP_REC_IDENTITY			0x80
#define	MIP_REC_DEPENDS				0x81
#define	MIP_REC_SETUP				0x82

int verbose = 0;
int compLevel = 1;
int fdOut;

clock_t totalLZ = 0;
clock_t totalHuff = 0;

uint8_t blockBuffer[BLOCK_SIZE];
uint64_t blockPut;

/**
 * Describes a segment produced from LZ77 encoding.
 */
typedef struct LZSegment_ LZSegment;
struct LZSegment_
{
	/**
	 * Next segment.
	 */
	LZSegment*				next;
	
	/**
	 * Size of this segment.
	 */
	size_t					size;
	
	/**
	 * Pointer to literal data. If NULL, then this is a backreference instead of a literal
	 * segment.
	 */
	uint8_t*				data;
	
	/**
	 * If data is NULL, then this is the number of bytes we must look back to find the
	 * string we are looking for, minus 'size'. So if this is zero, look back 'size'
	 * bytes; if this is 1, look back 'size+1' bytes.
	 */
	size_t					lookback;
};

/**
 * Represents a bitstream writer.
 */
typedef struct
{
	/**
	 * Pointer to current byte.
	 */
	uint8_t*				put;
	
	/**
	 * Size of buffer left (in bytes).
	 */
	size_t					sizeLeft;
	
	/**
	 * Number of bits written.
	 */
	size_t					bitsWritten;
	
	/**
	 * Mask for the next bit to write.
	 */
	uint8_t					mask;
} BitStreamWriter;

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

/**
 * MIP header.
 */
typedef struct
{
	uint8_t			mhMagic[3];
	uint8_t			mhFlags;
} MIPHeader;

/**
 * MIP identity record.
 */
typedef struct
{
	uint64_t recSize;
	uint8_t recType;
	uint8_t verRC;
	uint8_t verPatchLevel;
	uint8_t verMinor;
	uint8_t verMajor;
	char pkgName[];
} __attribute__ ((packed)) MIPIdentity;

/**
 * MIP dependency record.
 */
typedef struct
{
	uint64_t recSize;
	uint8_t recType;
	uint8_t verRC;
	uint8_t verPatchLevel;
	uint8_t verMinor;
	uint8_t verMajor;
	char pkgName[];
} __attribute__ ((packed)) MIPDependency;

/**
 * MIP post-install (setup) script record.
 */
typedef struct
{
	uint64_t recSize;
	uint8_t recType;
	char setupPath[];
} __attribute__ ((packed)) MIPSetup;

size_t lzCountMatch(const uint8_t *a, const uint8_t *b, size_t max)
{
	size_t count = 0;
	while (*a++ == *b++ && count < max) count++;
	return count;
};

ssize_t lzSearch(const uint8_t *haystack, size_t haysize, const uint8_t *text, size_t textSize, size_t *sizeOut)
{
	if (haysize < 4) return -1;
	
	ssize_t bestPos = -1;
	size_t bestSize = 0;
	
	size_t i;
	for (i=0; i<haysize-4; i++)
	{
		if (haystack[i] != text[0]) continue;
		
		size_t max = haysize - i;
		if (max > textSize) max = textSize;
		
		size_t count = lzCountMatch(&haystack[i], text, max);
		
		if (count > bestSize && count > 4)
		{
			bestPos = i;
			bestSize = count;
		};
	};
	
	*sizeOut = bestSize;
	return bestPos;
};

int putbit(BitStreamWriter *bsw, int bit)
{
	if (bsw->mask == 0)
	{
		if (bsw->sizeLeft == 0) return -1;
		bsw->put++;
		bsw->sizeLeft--;
		bsw->mask = 0x80;
	};
	
	if (bit)
	{
		*(bsw->put) |= bsw->mask;
	}
	else
	{
		*(bsw->put) &= ~bsw->mask;
	};
	
	bsw->mask >>= 1;
	bsw->bitsWritten++;
	return 0;
};

/**
 * Encode the specified block into a linked list of LZSegments, and return the head.
 */
LZSegment* lzEncode(const uint8_t *scan, size_t size)
{
	clock_t start = clock();
	size_t segstart = 0;
	size_t i = 0;
	
	LZSegment *first = NULL;
	LZSegment *last = NULL;
	
	size_t maxLookback = 0x1000;
	
	while (1)
	{
		if (i == size)
		{
			if (segstart != i)
			{
				LZSegment *final = (LZSegment*) malloc(sizeof(LZSegment));
				final->next = NULL;
				final->size = i - segstart;
				final->data = (uint8_t*) malloc(i - segstart);
				memcpy(final->data, &scan[segstart], i-segstart);
				
				if (last == NULL)
				{
					first = last = final;
				}
				else
				{
					last->next = final;
				};
			};
			
			break;
		};
		
		if (i >= 8)
		{
			const uint8_t *haystack = scan;
			size_t max = i;
			
			if (i >= maxLookback)
			{
				haystack = &scan[i-maxLookback];
				max = maxLookback;
			};
			
			size_t matchSize;
			ssize_t matchPos = lzSearch(haystack, max, &scan[i], size-i, &matchSize);
			
			if (matchPos != -1)
			{
				if (segstart != i)
				{
					LZSegment *new = (LZSegment*) malloc(sizeof(LZSegment));
					new->next = NULL;
					new->size = i - segstart;
					new->data = (uint8_t*) malloc(i - segstart);
					memcpy(new->data, &scan[segstart], i-segstart);
				
					if (last == NULL)
					{
						first = last = new;
					}
					else
					{
						last->next = new;
						last = new;
					};
				};
				
				LZSegment *ref = (LZSegment*) malloc(sizeof(LZSegment));
				ref->next = NULL;
				ref->size = matchSize;
				ref->data = NULL;
				
				const uint8_t *ptr = &haystack[matchPos];
				const uint8_t *me = &scan[i];
				ref->lookback = (size_t)(me - ptr) - matchSize;
				
				if (last == NULL)
				{
					first = last = ref;
				}
				else
				{
					last->next = ref;
					last = ref;
				};
				
				i += matchSize;
				segstart = i;
				continue;
			};
		};
		
		i++;
	};
	
	clock_t end = clock();
	totalLZ += (end-start);
	return first;
};

int riceEncode(uint8_t r, BitStreamWriter *bsw, uint16_t *array, size_t elements)
{
	uint16_t mask = (1 << r) - 1;
	
	int ok = 1;
	int i;
	for (i=0; i<elements; i++)
	{
		uint16_t q = array[i] >> r;
		uint16_t m = array[i] & mask;
		
		while (q--)
		{
			if (putbit(bsw, 1) != 0)
			{
				ok = 0;
				break;
			};
		};
		
		if (putbit(bsw, 0) != 0)
		{
			ok = 0;
			break;
		};
		
		uint8_t bitcount = r;
		while (bitcount--)
		{
			uint8_t bit = m & (1 << bitcount);
			if (putbit(bsw, bit) != 0)
			{
				ok = 0;
				break;
			};
		};
	};
	
	return ok;
};

MinHeap* heap;
HuffNode* huffTree;
HuffNode* byteNodes[258];

void addToHeap(HuffNode *node)
{
	MinHeap *new = (MinHeap*) malloc(sizeof(MinHeap));
	new->node = node;
	
	if (heap == NULL)
	{
		new->prev = new->next = NULL;
		heap = new;
		return;
	};
	
	if (heap->node->freq >= node->freq)
	{
		new->next = heap;
		heap->prev = new;
		new->prev = NULL;
		heap = new;
		return;
	};
	
	MinHeap *scan = heap;
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

/**
 * Output a huffman-encoded symbol to a bitstream writer. Return 0 if successful, -1 if not enough space.
 */
int huffWrite(BitStreamWriter *bsw, int symbol)
{
	int bits[64];
	int *put = &bits[64];
	*--put = -1;
	
	HuffNode *node = byteNodes[symbol];
	while (node->parent != NULL)
	{
		if (node->parent->children[1] == node)
		{
			*--put = 1;
		}
		else
		{
			*--put = 0;
		};
		
		node = node->parent;
	};
	
	while (*put != -1)
	{
		if (putbit(bsw, *put++) != 0) return -1;
	};
	
	return 0;
};

/**
 * Perform Huffman coding on the specified list of LZ77 segments. Returns the size of compressed data on
 * success, or 0 on error (e.g. the data is not compressible).
 */
size_t huffCompress(LZSegment *seglist, void *buffer, size_t bufsize)
{
	clock_t start = clock();
	if (verbose) printf("Computing frequencies...\n");
	
	uint16_t freqTable[257];
	uint64_t freqRefs = 0;
	memset(freqTable, 0, sizeof(freqTable));
	
	LZSegment *seg;
	for (seg=seglist; seg!=NULL; seg=seg->next)
	{
		if (seg->data == NULL)
		{
			freqRefs++;
		}
		else
		{
			size_t i;
			for (i=0; i<seg->size; i++)
			{
				freqTable[seg->data[i]]++;
			};
		};
	};
	
	freqTable[256] = freqRefs;
	
	if (verbose) printf("Dumping frequency table:\n");
	int i;
	for (i=0; i<257; i++)
	{
		if (verbose) printf("  Symbol [%d] appears %hu times\n", i, freqTable[i]);
	};
	
	if (verbose) printf("Encoding frequency table...\n");
	
	uint8_t *bitspec = (uint8_t*) buffer;
	uint8_t *rspec = bitspec + 1;
	
	uint8_t bestR = 0;
	size_t bestRbits = 0;
	
	BitStreamWriter bsw;
	uint8_t r;
	for (r=0; r<16; r++)
	{
		// reset BSW
		bsw.put = rspec + 1;
		bsw.sizeLeft = bufsize - 3;
		bsw.bitsWritten = 0;
		bsw.mask = 0x80;
		
		// encode the frequencies
		int ok = riceEncode(r, &bsw, freqTable, 257);
		
		if (ok)
		{
			if (bsw.bitsWritten < bestRbits || bestRbits == 0)
			{
				bestR = r;
				bestRbits = bsw.bitsWritten;
			};
		};
	};
	
	if (verbose) printf("The best R found was %hhu, encoding in %lu bits\n", bestR, bestRbits);
	
	if (verbose) printf("Determining bits required for lookback/size values\n");
	size_t lookbackBits = 0;
	size_t sizeBits = 0;
	
	for (seg=seglist; seg!=NULL; seg=seg->next)
	{
		if (seg->data == NULL)
		{
			while (seg->lookback >= (1 << lookbackBits)) lookbackBits++;
			while (seg->size >= (1 << sizeBits)) sizeBits++;
		};
	};
	
	if (verbose) printf("Result: %lu lookback bits, %lu size bits\n", lookbackBits, sizeBits);
	
	*bitspec = (uint8_t) ((lookbackBits << 4) | sizeBits);
	*rspec = bestR;
	
	// reset BSW
	bsw.put = rspec + 1;
	bsw.sizeLeft = bufsize - 3;
	bsw.bitsWritten = 0;
	bsw.mask = 0x80;
	
	// encode the frequency table
	riceEncode(bestR, &bsw, freqTable, 257);
	
	// prepare the heap
	heap = (MinHeap*) malloc(sizeof(MinHeap));
	heap->prev = heap->next = NULL;
	heap->node = (HuffNode*) malloc(sizeof(HuffNode));
	heap->node->children[0] = heap->node->children[1] = NULL;
	heap->node->parent = NULL;
	heap->node->freq = 1;
	heap->node->value = 257;
	byteNodes[257] = heap->node;
	
	for (i=0; i<257; i++)
	{
		HuffNode *node = (HuffNode*) malloc(sizeof(HuffNode));
		node->children[0] = node->children[1] = NULL;
		node->parent = NULL;
		node->freq = freqTable[i];
		node->value = i;
	
		addToHeap(node);
		byteNodes[i] = node;
	};
	
	// print them out
	if (verbose) printf("Sorted symbols:\n");
	MinHeap *hp;
	for (hp=heap; hp!=NULL; hp=hp->next)
	{
		if (verbose) printf("  Symbol %d with frequency %lu\n", hp->node->value, hp->node->freq);
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
		
		addToHeap(node);
	};
	
	huffTree = heap->node;
	free(heap);
	
	// encode the data
	for (seg=seglist; seg!=NULL; seg=seg->next)
	{
		if (seg->data != NULL)
		{
			size_t i;
			for (i=0; i<seg->size; i++)
			{
				if (huffWrite(&bsw, (int) seg->data[i]) != 0) return 0;
			};
		}
		else
		{
			// encode the REPEAT symbol
			if (huffWrite(&bsw, 256) != 0) return 0;
			
			// encode the lookback using however many bits we promised
			size_t count = lookbackBits;
			while (count--)
			{
				if (putbit(&bsw, seg->lookback & (1 << count)) != 0) return 0;
			};
			
			// encode the size using however many bits we promised
			count = sizeBits;
			while (count--)
			{
				if (putbit(&bsw, seg->size & (1 << count)) != 0) return 0;
			};
		};
	};
	
	// encode the end symbol
	if (huffWrite(&bsw, 257) != 0) return 0;
	
	// pad with zeroes
	while (bsw.bitsWritten % 8) putbit(&bsw, 0);
	
	// thanks
	if (verbose) printf("Encoding complete using %lu bits (%lu bytes)\n", bsw.bitsWritten, bsw.bitsWritten/8);
	clock_t end = clock();
	totalHuff += (end-start);
	return bsw.bitsWritten/8 + 2;
};

void writeBigInt(int fd, uint64_t val)
{
	uint8_t buf[32];
	uint8_t *put = &buf[32];
	size_t size = 0;
	uint8_t flag = 0;
	
	do
	{
		*--put = (val & 0x7F) | flag;
		flag = 0x80;
		size++;
		val >>= 7;
	} while (val != 0);
	
	write(fd, put, size);
};

void flushBlock()
{
	// do nothing if the buffer is currently empty
	if (blockPut == 0) return;
	
	if (compLevel < 1)
	{
		write(fdOut, "\0", 1);
		writeBigInt(fdOut, blockPut);
		write(fdOut, blockBuffer, blockPut);
	}
	else
	{
		LZSegment *list;
		if (compLevel < 2)
		{
			list = (LZSegment*) malloc(sizeof(LZSegment));
			list->next = NULL;
			list->data = blockBuffer;
			list->size = blockPut;
		}
		else list = lzEncode(blockBuffer, blockPut);
		
		uint8_t compbuf[BLOCK_SIZE];
		size_t compsize = huffCompress(list, compbuf, blockPut);
		
		if (compsize == 0)
		{
			write(fdOut, "\0", 1);
			writeBigInt(fdOut, blockPut);
			write(fdOut, blockBuffer, blockPut);
		}
		else
		{
			write(fdOut, "\1", 1);
			writeBigInt(fdOut, compsize);
			write(fdOut, compbuf, compsize);
		};
	};
	
	// clear the buffer again
	blockPut = 0;
};

void output(const void *data, size_t size)
{
	const uint8_t *scan = (const uint8_t*) data;
	
	while (size)
	{
		if (blockPut == BLOCK_SIZE)
		{
			flushBlock();
		};
		
		size_t putNow = BLOCK_SIZE - blockPut;
		if (putNow > size) putNow = size;
		
		memcpy(&blockBuffer[blockPut], scan, putNow);
		blockPut += putNow;
		scan += putNow;
		size -= putNow;
	};
};

void writeDir(const char *prefix, const char *dir)
{
	DIR *dirp = opendir(dir);
	if (dirp == NULL)
	{
		perror(dir);
		exit(1);
	};
	
	struct dirent *ent;
	while ((ent = readdir(dirp)) != NULL)
	{
		if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0) continue;
		
		char *subprefix = (char*) malloc(strlen(prefix) + strlen(ent->d_name) + 2);
		char *fullpath = (char*) malloc(strlen(dir) + strlen(ent->d_name) + 2);
		
		sprintf(subprefix, "%s/%s", prefix, ent->d_name);
		sprintf(fullpath, "%s/%s", dir, ent->d_name);
		
		struct stat st;
		if (lstat(fullpath, &st) != 0)
		{
			perror(fullpath);
			exit(1);
		};
		
		if (S_ISLNK(st.st_mode))
		{
			char target[256];
			target[readlink(fullpath, target, 255)] = 0;
			
			uint64_t recSize = 10 + strlen(target) + strlen(subprefix);
			output(&recSize, 8);
			output("\x02", 1);
			
			output(subprefix, strlen(subprefix));
			output("\0", 1);
			output(target, strlen(target));
		}
		else if (S_ISDIR(st.st_mode))
		{
			uint64_t recSize = 12 + strlen(subprefix);
			output(&recSize, 8);
			output("\x01\x00", 2);
			uint16_t mode = st.st_mode & 07777;
			output(&mode, 2);
			
			output(subprefix, strlen(subprefix));
			
			writeDir(subprefix, fullpath);
		}
		else
		{
			// file
			uint64_t recSize = 13 + strlen(subprefix) + st.st_size;
			output(&recSize, 8);
			output("\x03\x00", 2);
			uint16_t mode = st.st_mode & 07777;
			output(&mode, 2);
			output(subprefix, strlen(subprefix)+1);		// include NUL
			
			void *buffer = malloc(st.st_size);
			int fd = open(fullpath, O_RDONLY);
			if (fd == -1)
			{
				perror(fullpath);
				exit(1);
			};
			
			read(fd, buffer, st.st_size);
			close(fd);
			
			output(buffer, st.st_size);
			free(buffer);
		};
		
		free(subprefix);
		free(fullpath);
	};
	
	closedir(dirp);
};

int main(int argc, char *argv[])
{
	if (argc < 3)
	{
		fprintf(stderr, "USAGE:\t%s sysroot filename [--option=value]...\n", argv[0]);
		fprintf(stderr, "\tCreate a MIP package using the specified\n");
		fprintf(stderr, "\tsysroot, in the given filename.\n");
		return 1;
	};
	
	fdOut = open(argv[2], O_WRONLY | O_CREAT | O_TRUNC, 0644);
	if (fdOut == -1)
	{
		fprintf(stderr, "%s: cannot open %s: %s\n", argv[0], argv[2], strerror(errno));
		return 1;
	};
	
	verbose = (getenv("MIP_VERBOSE") != NULL);
	
	char *complvlspec = getenv("MIP_COMP_LEVEL");
	if (complvlspec != NULL)
	{
		sscanf(complvlspec, "%d", &compLevel);
	};
	
	MIPHeader head;
	memcpy(head.mhMagic, "MIP", 3);
	head.mhFlags = 0x80;
	
	write(fdOut, &head, sizeof(MIPHeader));
	
	// write "required features" (currently just 0)
	uint64_t features = 0;
	output(&features, 8);
	
	// parse command-line options and write appropriate records
	char **scan = &argv[3];
	while (*scan != NULL)
	{
		if (strcmp(*scan, "-i") == 0 || strcmp(*scan, "--identity") == 0)
		{
			scan++;
			char *name = *scan;
			if (name == NULL)
			{
				fprintf(stderr, "%s: too few arguments to -i / --identity\n", argv[0]);
				return 1;
			};
			
			scan++;
			char *verspec = *scan;
			if (verspec == NULL)
			{
				fprintf(stderr, "%s: too few argument to -i / --identity\n", argv[0]);
				return 1;
			};
			
			scan++;
			
			MIPIdentity *id = (MIPIdentity*) malloc(sizeof(MIPIdentity) + strlen(name));
			memset(id, 0, sizeof(MIPIdentity) + strlen(name));
			id->recSize = sizeof(MIPIdentity) + strlen(name);
			id->recType = MIP_REC_IDENTITY;
			id->verRC = 0xFF;
			id->verPatchLevel = 0;
			id->verMinor = 0;
			id->verMajor = 1;
			sscanf(verspec, "%hhu.%hhu.%hhu-rc%hhu", &id->verMajor, &id->verMinor, &id->verPatchLevel, &id->verRC);
			memcpy(id->pkgName, name, strlen(name));
			
			output(id, id->recSize);
		}
		else if (strcmp(*scan, "-d") == 0 || strcmp(*scan, "--depends") == 0)
		{
			scan++;
			char *name = *scan;
			if (name == NULL)
			{
				fprintf(stderr, "%s: too few arguments to -d / --depends\n", argv[0]);
				return 1;
			};
			
			scan++;
			char *verspec = *scan;
			if (verspec == NULL)
			{
				fprintf(stderr, "%s: too few argument to -d / --depends\n", argv[0]);
				return 1;
			};
			
			scan++;
			
			MIPDependency *dep = (MIPDependency*) malloc(sizeof(MIPDependency) + strlen(name));
			memset(dep, 0, sizeof(MIPDependency) + strlen(name));
			dep->recSize = sizeof(MIPDependency) + strlen(name);
			dep->recType = MIP_REC_DEPENDS;
			dep->verRC = 0xFF;
			dep->verPatchLevel = 0;
			dep->verMinor = 0;
			dep->verMajor = 1;
			sscanf(verspec, "%hhu.%hhu.%hhu-rc%hhu", &dep->verMajor, &dep->verMinor, &dep->verPatchLevel, &dep->verRC);
			memcpy(dep->pkgName, name, strlen(name));
			
			output(dep, dep->recSize);
		}
		else if (memcmp(*scan, "--setup=", 8) == 0)
		{
			char *path = (*scan) + 8;
			scan++;
			
			MIPSetup *setup = (MIPSetup*) malloc(sizeof(MIPSetup) + strlen(path));
			setup->recSize = sizeof(MIPSetup) + strlen(path);
			setup->recType = MIP_REC_SETUP;
			memcpy(setup->setupPath, path, strlen(path));
			
			output(setup, setup->recSize);
		}
		else
		{
			fprintf(stderr, "%s: unknown command-line option: `%s'\n", argv[0], *scan);
			return 1;
		};
	};
	
	// end of control area
	output("\x09\x00\x00\x00\x00\x00\x00\x00\xFF", 9);
	
	// write package contents
	writeDir("", argv[1]);
	
	// finished, flush
	flushBlock();
	close(fdOut);
	return 0;
};
