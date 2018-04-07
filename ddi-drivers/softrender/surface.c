/*
	Madd Software Renderer

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

#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#include "surface.h"

typedef struct
{
	int width, height;
	DDIPixelFormat format;
} SharedSurfaceMeta;

typedef uint8_t	DDIByteVector __attribute__ ((vector_size(4)));
typedef int DDIIntVector __attribute__ ((vector_size(16)));
typedef uint32_t DDIU32Vector __attribute__ ((vector_size(16)));

static void fastCopy(void *dest_, const void *src_, size_t sz)
{
	// align, and fall back to memcpy if impossible
	if (((uint64_t)dest_ & 0xF) != ((uint64_t)src_ & 0xF))
	{
		memcpy(dest_, src_, sz);
		return;
	};
	
	register char *destChar = (char*) dest_;
	register const char *srcChar = (const char*) src_;
	
	while (((uint64_t)destChar & 0xF) && sz) {*destChar++ = *srcChar++; sz--;};
	
	register DDIIntVector *destVec = (DDIIntVector*) destChar;
	register const DDIIntVector *srcVec = (const DDIIntVector*) srcChar;
	while (sz >= 16) {*destVec++ = *srcVec++; sz -= 16;};
	
	destChar = (char*) destVec;
	srcChar = (const char*) srcVec;
	while (sz--) *destChar++ = *srcChar++;
};

static uint8_t* srCreateSharedFile(uint32_t *idOut, DDISurface *target)
{	
	while (1)
	{
		uint32_t val = _glidix_unique();
		if (val == 0) continue;
		
		char shpath[256];
		sprintf(shpath, "/run/shsurf/%08X", val);
		
		mode_t mode = 0600;
		if (geteuid() == 0)
		{
			mode = 0777;
		};
		
		int fd = open(shpath, O_RDWR | O_CREAT | O_EXCL, mode);
		if (fd == -1)
		{
			if (errno == EEXIST)
			{
				continue;
			}
			else
			{
				perror("srCreateSharedFile");
				return NULL;
			};
		};
		
		SharedSurfaceMeta meta;
		meta.width = target->width;
		meta.height = target->height;
		memcpy(&meta.format, &target->format, sizeof(DDIPixelFormat));
		
		ftruncate(fd, ddiGetFormatDataSize(&target->format, target->width, target->height) + 0x1000);
		pwrite(fd, &meta, sizeof(SharedSurfaceMeta), 0);
		
		void *result = mmap(NULL, ddiGetFormatDataSize(&target->format, target->width, target->height),
					PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0x1000);
		close(fd);
		
		if (result == MAP_FAILED && target->width != 0 && target->height != 0)
		{
			return NULL;
		};
		
		*idOut = val;
		return (uint8_t*) result;
	};
};

int srCreateSurface(void *drvctx, DDISurface *surface, char *data)
{
	(void)drvctx;
	
	if (surface->flags & DDI_SHARED)
	{
		surface->data = srCreateSharedFile(&surface->id, surface);
		if (surface->data == NULL)
		{
			return -1;
		};
	}
	else
	{
		surface->data = (uint8_t*) mmap(NULL, ddiGetFormatDataSize(&surface->format, surface->width, surface->height),
						PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	};
	if (data != NULL) memcpy(surface->data, data, ddiGetFormatDataSize(&surface->format, surface->width, surface->height));
	
	return 0;
};

int srOpenSurface(void *drvctx, DDISurface *surface)
{
	char pathname[256];
	sprintf(pathname, "/run/shsurf/%08X", surface->id);
	
	int fd = open(pathname, O_RDWR);
	if (fd == -1)
	{
		return -1;
	};
	
	SharedSurfaceMeta meta;
	pread(fd, &meta, sizeof(SharedSurfaceMeta), 0);
	memcpy(&surface->format, &meta.format, sizeof(DDIPixelFormat));
	
	surface->width = meta.width;
	surface->height = meta.height;
	surface->flags = DDI_SHARED;
	surface->data = (uint8_t*) mmap(NULL, ddiGetFormatDataSize(&surface->format, surface->width, surface->height),
					PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0x1000);
	close(fd);
	
	return 0;
};

/**
 * Don't question the use of the "register" keyword here. It causes the function to be approximately
 * 2 times faster, when compiling with GCC.
 */
void srBlit(void *drvctx, DDISurface *src, int srcX, int srcY, DDISurface *dest, int destX, int destY, int width, int height)
{
	(void)drvctx;
	
	if (src->format.alphaMask == 0)
	{
		ddiOverlay(src, srcX, srcY, dest, destX, destY, width, height);
	};
	
	int alphaIndex = ddiGetIndexForMask(src->format.alphaMask);

	if (srcX < 0)
	{
		destX -= srcX;
		width += srcX;
		srcX = 0;
	};
	
	if (srcY < 0)
	{
		destY -= srcY;
		height += srcY;
		srcY = 0;
	};

	if (destX < 0)
	{
		srcX -= destX;
		width += destX;
		destX = 0;
	};
	
	if (destY < 0)
	{
		srcY -= destY;
		height += destX;
		destY = 0;
	};

	// clip the rectangle to make sure it can fit on both surfaces
	if ((srcX >= src->width) || (srcY >= src->height))
	{
		// do not need to do anything
		return;
	};
	
	if ((destX+(int)width <= 0) || (destY+(int)height <= 0))
	{
		return;
	};
	
	if ((srcX+width) > src->width)
	{
		if (src->width <= srcX)
		{
			return;
		};
		
		width = src->width - srcX;
	};
	
	if ((srcY+height) > src->height)
	{
		if (src->height <= srcY)
		{
			return;
		};
		
		height = src->height - srcY;
	};
	
	if ((destX >= dest->width) || (destY >= dest->height))
	{
		// do not need to do anything
		return;
	};
	
	if ((destX+width) > dest->width)
	{
		if (dest->width <= destX)
		{
			return;
		};
		
		width = dest->width - destX;
	};
	
	if ((destY+height) > dest->height)
	{
		if (dest->height <= destY)
		{
			return;
		};
		
		height = dest->height - destY;
	};
	
	if (width < 0 || height < 0) return;
	
	// calculate offsets
	register size_t pixelSize = src->format.bpp + src->format.pixelSpacing;
	register size_t srcScanlineSize = src->format.scanlineSpacing + pixelSize * src->width;
	register size_t destScanlineSize = dest->format.scanlineSpacing + pixelSize * dest->width;
	
	register uint8_t *scan = (uint8_t*) src->data + pixelSize * srcX + srcScanlineSize * srcY;
	register uint8_t *put = (uint8_t*) dest->data + pixelSize * destX + destScanlineSize * destY;
	
	for (; height; height--)
	{
		register size_t count = width;
		register uint8_t *scanStart = scan;
		register uint8_t *putStart = put;
		
		while (count--)
		{
			register int srcAlpha = (int) scan[alphaIndex];
			register int dstAlpha = (int) put[alphaIndex];
			register int outAlpha = srcAlpha + (int) dstAlpha * (255 - srcAlpha) / 255;

			if (outAlpha == 0)
			{
				*((uint32_t*)put) = 0;
			}
			else
			{
				DDIIntVector vdst = {put[0], put[1], put[2], put[3]};
				DDIIntVector vsrc = {scan[0], scan[1], scan[2], scan[3]};
				DDIIntVector result = (
					(vsrc * srcAlpha)/255
					+ (vdst * dstAlpha * (255-srcAlpha))/(255*255)
				)*255/outAlpha;
				
				put[0] = (uint8_t) result[0];
				put[1] = (uint8_t) result[1];
				put[2] = (uint8_t) result[2];
				put[3] = (uint8_t) result[3];
				put[alphaIndex] = outAlpha;
			};

			scan += pixelSize;
			put += pixelSize;
		};
		
		scan = scanStart + srcScanlineSize;
		put = putStart + destScanlineSize;
	};
};

void srOverlay(void *drvctx, DDISurface *src, int srcX, int srcY, DDISurface *dest, int destX, int destY, int width, int height)
{
	(void)drvctx;

	if (srcX < 0)
	{
		destX -= srcX;
		width += srcX;
		srcX = 0;
	};
	
	if (srcY < 0)
	{
		destY -= srcY;
		height += srcY;
		srcY = 0;
	};

	if (destX < 0)
	{
		srcX -= destX;
		width += destX;
		destX = 0;
	};
	
	if (destY < 0)
	{
		srcY -= destY;
		height += destX;
		destY = 0;
	};

	// clip the rectangle to make sure it can fit on both surfaces
	if ((srcX >= src->width) || (srcY >= src->height))
	{
		// do not need to do anything
		return;
	};
	
	if ((destX+(int)width <= 0) || (destY+(int)height <= 0))
	{
		return;
	};
	
	if ((srcX+width) > src->width)
	{
		if (src->width <= srcX)
		{
			return;
		};
		
		width = src->width - srcX;
	};
	
	if ((srcY+height) > src->height)
	{
		if (src->height <= srcY)
		{
			return;
		};
		
		height = src->height - srcY;
	};
	
	if ((destX >= dest->width) || (destY >= dest->height))
	{
		// do not need to do anything
		return;
	};
	
	if ((destX+width) > dest->width)
	{
		if (dest->width <= destX)
		{
			return;
		};
		
		width = dest->width - destX;
	};
	
	if ((destY+height) > dest->height)
	{
		if (dest->height <= destY)
		{
			return;
		};
		
		height = dest->height - destY;
	};
	
	if (width < 0 || height < 0) return;
	
	// calculate offsets
	register size_t pixelSize = src->format.bpp + src->format.pixelSpacing;
	register size_t srcScanlineSize = src->format.scanlineSpacing + pixelSize * src->width;
	register size_t destScanlineSize = dest->format.scanlineSpacing + pixelSize * dest->width;
	
	register uint8_t *scan = (uint8_t*) src->data + pixelSize * srcX + srcScanlineSize * srcY;
	register uint8_t *put = (uint8_t*) dest->data + pixelSize * destX + destScanlineSize * destY;
	
	for (; height; height--)
	{
		fastCopy(put, scan, pixelSize * width);
		scan += srcScanlineSize;
		put += destScanlineSize;
	};
};

static void srFill(void *dest, void *src, uint64_t unit, uint64_t count)
{
	// what else can we do, but optimise for the most obvious case?
	if (unit == 4)
	{
		register uint32_t *dest32 = (uint32_t*) dest;
		register uint32_t val32 = *((uint32_t*) src);
		
		while ((uint64_t)dest32 & 0xF)
		{
			if (count--) *dest32++ = val32;
			else return;
		};
		
		if (count >= 4)
		{
			DDIU32Vector *destVec = (DDIU32Vector*) dest32;
			DDIU32Vector valVec = {val32, val32, val32, val32};
			
			while (count >= 4) {*destVec++ = valVec; count -= 4;};
			dest32 = (uint32_t*) destVec;
		};
		
		while (count--)
		{
			*dest32++ = val32;
		};
	}
	else
	{
		while (count--)
		{
			fastCopy(dest, src, unit);
			dest += unit;
		};
	};
};

void srRect(void *drvctx, DDISurface *surface, int x, int y, int width, int height, DDIColor *color)
{
	(void)drvctx;

	if (x < 0)
	{
		width += x;
		x = 0;
	};

	if (y < 0)
	{
		height += y;
		y = 0;
	};
	
	if ((x >= surface->width) || (y >= surface->height))
	{
		// do not need to do anything
		return;
	};
	
	if ((x+width) > surface->width)
	{
		width = surface->width - x;
	};
	
	if ((y+height) > surface->height)
	{
		height = surface->height - y;
	};
	
	if (width < 0 || height < 0) return;
	
	size_t pixelSize = surface->format.bpp + surface->format.pixelSpacing;
	size_t scanlineSize = surface->format.scanlineSpacing + pixelSize * surface->width;
	
	uint32_t pixbuf[8];
	ddiColorToPixel(&pixbuf[0], &surface->format, color);
	
	unsigned char *put = surface->data + scanlineSize * y + pixelSize * x;
	for (; height; height--)
	{
		srFill(put, pixbuf, pixelSize, width);
		put += scanlineSize;
	};
};
