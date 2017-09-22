/*
	Glidix Display Device Interface (DDI)

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

#ifdef __glidix__
#include <sys/glidix.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#endif

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <png.h>
#include <ft2build.h>
#include FT_FREETYPE_H

#include "libddi.h"

typedef uint8_t	DDIByteVector __attribute__ ((vector_size(4)));
typedef int DDIIntVector __attribute__ ((vector_size(16)));

typedef struct PenSegment_
{
	DDISurface *surface;
	struct PenSegment_ *next;
	int firstCharPos;			// position of the first character in the segment, within the original text
	size_t numChars;			// number of characters in segment
	int *widths;				// width of each character
	
	// where the segment was last drawn (initialized by ddiExecutePen).
	int drawX, drawY;
	
	// baseline position
	int baselineY;
	DDIColor background;
} PenSegment;

typedef struct PenLine_
{
	PenSegment *firstSegment;
	int currentWidth;
	int maxHeight;
	int alignment;
	int lineHeight;				// %
	int drawY;
	int baselineY;
	struct PenLine_ *next;
} PenLine;

/**
 * Represents a cache glyph; see the glyph hashtable in "DDIFont".
 */
typedef struct DDIGlyphCache_
{
	/**
	 * Link.
	 */
	struct DDIGlyphCache_* next;
	
	/**
	 * The Unicode codepoint.
	 */
	long codepoint;
	
	/**
	 * The rendered bitmap.
	 */
	uint8_t* bitmap;
	
	/**
	 * Parameters.
	 */
	unsigned int width, height, pitch;
	
	/**
	 * Advance values (in 1/64 units; taken straight from FreeType).
	 */
	int advanceX, advanceY;
	
	/**
	 * Bitmap top/left.
	 */
	int bitmap_top, bitmap_left;
} DDIGlyphCache;

struct DDIFont_
{
	/**
	 * FreeType library handle.
	 */
	FT_Library lib;

	/**
	 * The FreeType face.
	 */
	FT_Face face;
	
	/**
	 * Font size.
	 */
	int size;
	
	/**
	 * Hashtable of cached glyph bitmaps. The hash is the bottom 5 bits of the codepoint.
	 */
	DDIGlyphCache* glyphCache[32];
};

struct DDIPen_
{
	/**
	 * The format of the surface onto which we will draw (typically the screen format).
	 */
	DDIPixelFormat format;
	
	/**
	 * The font which we are currently using.
	 */
	DDIFont *font;
	
	/**
	 * The bounding box onto which to draw.
	 */
	int x, y, width, height;
	
	/**
	 * The current scroll position.
	 */
	int scrollX, scrollY;
	
	/**
	 * Nonzero if text should be word-wrapped automatically (default = 1).
	 */
	int wrap;
	
	/**
	 * Text alignment (default = DDI_ALIGN_LEFT).
	 */
	int align;
	
	/**
	 * Letter spacing and line height. Letter spacing is given in pixels (default = 0), while the line height
	 * is given as a percentage (default = 100).
	 */
	int letterSpacing;
	int lineHeight;
	
	/**
	 * Cursor position, in character units; -1 (default) means do not draw the cursor.
	 */
	int cursorPos;
	
	/**
	 * The current writer position (i.e. how many unicode characters were written to the pen).
	 */
	int writePos;
	
	/**
	 * Masking character (0 = none, 1 = default, everything else = codepoint for mask).
	 */
	long mask;
	
	/**
	 * Background and foreground colors.
	 */
	DDIColor background;
	DDIColor foreground;
	
	/**
	 * Current line being layed out.
	 */
	PenLine *currentLine;
	
	/**
	 * Head of the line list.
	 */
	PenLine *firstLine;
};

typedef struct
{
	int width, height;
	DDIPixelFormat format;
} SharedSurfaceMeta;

static int ddiFD;

int ddiInit(const char *display, int oflag)
{
#ifdef __glidix__
	ddiFD = open(display, oflag | O_CLOEXEC);
	if (ddiFD == -1)
	{
		return -1;
	};
#endif
	return 0;
};

void ddiQuit()
{
#ifdef __glidix__
	close(ddiFD);
#endif
};

static size_t ddiGetSurfaceDataSize(DDISurface *surface)
{
	size_t scanlineLen = surface->format.scanlineSpacing
		+ (surface->format.bpp + surface->format.pixelSpacing) * surface->width;
	size_t size = scanlineLen * surface->height;
	
	// make sure the size is a multiple of 4 bytes as that helps with certain operations
	if (size & 3)
	{
		size &= ~((size_t)3);
		size += 4;
	};
	
	return size;
};

#ifdef __glidix__
DDISurface* ddiSetVideoMode(uint64_t res)
{
	DDIModeRequest req;
	req.res = res;
	
	if (ioctl(ddiFD, DDI_IOCTL_VIDEO_MODESET, &req) != 0)
	{
		return NULL;
	};
	
	DDISurface *surface = (DDISurface*) malloc(sizeof(DDISurface));
	memcpy(&surface->format, &req.format, sizeof(DDIPixelFormat));
	
	surface->width = (int) DDI_RES_WIDTH(req.res);
	surface->height = (int) DDI_RES_HEIGHT(req.res);
	surface->flags = 0;
	surface->id = 0;
	surface->data = (uint8_t*) mmap(NULL, ddiGetSurfaceDataSize(surface),
					PROT_READ | PROT_WRITE, MAP_SHARED, ddiFD, 0);
	
	if (surface->data == MAP_FAILED)
	{
		int errnum = errno;
		free(surface);
		errno = errnum;
		return NULL;
	};
	
	return surface;
};
#endif

size_t ddiGetFormatDataSize(DDIPixelFormat *format, unsigned int width, unsigned int height)
{
	size_t scanlineLen = format->scanlineSpacing
		+ (format->bpp + format->pixelSpacing) * width;
	size_t size = scanlineLen * height;
	
	// make sure the size is a multiple of 4 bytes as that helps with certain operations
	if (size & 3)
	{
		size &= ~((size_t)3);
		size += 4;
	};
	
	return size;
};

#ifdef __glidix__
static uint8_t* ddiCreateSharedFile(uint32_t *idOut, DDISurface *target)
{	
	while (1)
	{
		uint32_t val = _glidix_unique();
		if (val == 0) continue;
		
		char shpath[256];
		sprintf(shpath, "/run/shsurf/%08X", val);
		
		// TODO: we should probably think about how to manage the permissions better!
		// this must be accessible to other users because the GUI runs as root while
		// other applications do not.
		int fd = open(shpath, O_RDWR | O_CREAT | O_EXCL, 0666);
		if (fd == -1)
		{
			if (errno == EEXIST)
			{
				continue;
			}
			else
			{
				return NULL;
			};
		};
		
		SharedSurfaceMeta meta;
		meta.width = target->width;
		meta.height = target->height;
		memcpy(&meta.format, &target->format, sizeof(DDIPixelFormat));
		
		ftruncate(fd, ddiGetSurfaceDataSize(target) + 0x1000);
		pwrite(fd, &meta, sizeof(SharedSurfaceMeta), 0);
		
		void *result = mmap(NULL, ddiGetSurfaceDataSize(target), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0x1000);
		close(fd);
		
		if (result == MAP_FAILED)
		{
			return NULL;
		};
		
		*idOut = val;
		return (uint8_t*) result;
	};
};
#endif

DDISurface* ddiCreateSurface(DDIPixelFormat *format, unsigned int width, unsigned int height, char *data, unsigned int flags)
{
	DDISurface *surface = (DDISurface*) malloc(sizeof(DDISurface));
	memcpy(&surface->format, format, sizeof(DDIPixelFormat));
	
	surface->width = width;
	surface->height = height;
	surface->flags = flags;
	surface->id = 0;
	
	if ((flags & DDI_STATIC_FRAMEBUFFER) == 0)
	{
#ifdef __glidix__
		if (flags & DDI_SHARED)
		{
			surface->data = ddiCreateSharedFile(&surface->id, surface);
			if (surface->data == NULL)
			{
				free(surface);
				return NULL;
			};
		}
		else
		{
			surface->data = (uint8_t*) mmap(NULL, ddiGetSurfaceDataSize(surface), PROT_READ | PROT_WRITE,
							MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
		};
#else
		surface->data = (uint8_t*) malloc(ddiGetSurfaceDataSize(surface));
#endif
		if (data != NULL) memcpy(surface->data, data, ddiGetSurfaceDataSize(surface));
	}
	else
	{
		surface->data = (uint8_t*) data;
	};
	
	return surface;
};

#ifdef __glidix__
DDISurface *ddiOpenSurface(uint32_t id)
{
	char pathname[256];
	sprintf(pathname, "/run/shsurf/%08X", id);
	
	int fd = open(pathname, O_RDWR);
	if (fd == -1)
	{
		return NULL;
	};
	
	SharedSurfaceMeta meta;
	pread(fd, &meta, sizeof(SharedSurfaceMeta), 0);
	DDISurface *surface = (DDISurface*) malloc(sizeof(DDISurface));
	memcpy(&surface->format, &meta.format, sizeof(DDIPixelFormat));
	
	surface->width = meta.width;
	surface->height = meta.height;
	surface->flags = DDI_SHARED;
	surface->id = id;
	surface->data = (uint8_t*) mmap(NULL, ddiGetSurfaceDataSize(surface),
					PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0x1000);
	close(fd);
	
	return surface;
};
#endif

void ddiDeleteSurface(DDISurface *surface)
{
	if ((surface->flags & DDI_STATIC_FRAMEBUFFER) == 0)
	{
#if __glidix__
		munmap(surface->data, ddiGetSurfaceDataSize(surface));
#else
		free(surface->data);
#endif
	};
	
	free(surface);
};

static void ddiInsertWithMask(uint32_t *target, uint32_t mask, uint32_t val)
{
	if (mask == 0) return;
	while ((mask & 1) == 0)
	{
		mask >>= 1;
		val <<= 1;
	};
	
	(*target) |= val;
};

static void ddiColorToPixel(uint32_t *pixeldata, DDIPixelFormat *format, DDIColor *color)
{
	*pixeldata = 0;
	ddiInsertWithMask(pixeldata, format->redMask, color->red);
	ddiInsertWithMask(pixeldata, format->greenMask, color->green);
	ddiInsertWithMask(pixeldata, format->blueMask, color->blue);
	ddiInsertWithMask(pixeldata, format->alphaMask, color->alpha);
};

static void ddiFill(void *dest, void *src, uint64_t unit, uint64_t count)
{
	while (count--)
	{
		memcpy(dest, src, unit);
		dest += unit;
	};
};

void ddiFillRect(DDISurface *surface, int x, int y, unsigned int width, unsigned int height, DDIColor *color)
{
	while (x < 0)
	{
		x++;
		if (width == 0) return;
		width--;
	};
	
	while (y < 0)
	{
		y++;
		if (height == 0) return;
		height--;
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
	
	uint32_t pixel;
	ddiColorToPixel(&pixel, &surface->format, color);
	
	size_t pixelSize = surface->format.bpp + surface->format.pixelSpacing;
	size_t scanlineSize = surface->format.scanlineSpacing + pixelSize * surface->width;
	
	char pixbuf[32];
	memcpy(pixbuf, &pixel, 4);
	
	size_t offset = scanlineSize * y + pixelSize * x;
	unsigned char *put = surface->data + offset;
	for (; height; height--)
	{
		ddiFill(put, pixbuf, pixelSize, width);
		put += scanlineSize;
	};
};

void ddiOverlay(DDISurface *src, int srcX, int srcY, DDISurface *dest, int destX, int destY,
	unsigned int width, unsigned int height)
{
	// cannot copy from negative source coordinates
	if ((srcX < 0) || (srcY < 0))
	{
		return;
	};
	
	// we can copy to negative target coordinates with some messing around
	while (destX < 0)
	{
		srcX++;
		destX++;
		if (width < 0) return;
		width--;
	};
	
	while (destY < 0)
	{
		srcY++;
		destY++;
		if (height < 0) return;
		height--;
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
	
	// make sure the formats are the same
	if (memcmp(&src->format, &dest->format, sizeof(DDIPixelFormat)) != 0)
	{
		return;
	};
	
	// calculate offsets
	size_t pixelSize = src->format.bpp + src->format.pixelSpacing;
	size_t srcScanlineSize = src->format.scanlineSpacing + pixelSize * src->width;
	size_t destScanlineSize = dest->format.scanlineSpacing + pixelSize * dest->width;
	
	unsigned char *scan = src->data + pixelSize * srcX + srcScanlineSize * srcY;
	unsigned char *put = dest->data + pixelSize * destX + destScanlineSize * destY;
	
	for (; height; height--)
	{
		memcpy(put, scan, pixelSize * width);
		//memcpy(put, scan, pixelSize * width);
		scan += srcScanlineSize;
		put += destScanlineSize;
	};
};

#define	DDI_ERROR(msg)	if (error != NULL) *error = msg

DDISurface* ddiLoadPNG(const char *filename, const char **error)
{
	unsigned char header[8];
	
	FILE *fp = fopen(filename, "rb");
	if (fp == NULL)
	{
		DDI_ERROR("Could not open the PNG file");
		return NULL;
	};
	
	fread(header, 1, 8, fp);
	if (png_sig_cmp(header, 0, 8))
	{
		DDI_ERROR("Invalid header in PNG file");
		fclose(fp);
		return NULL;
	};
	
	png_structp png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	if (!png_ptr)
	{
		DDI_ERROR("Could not allocate PNG read struct");
		fclose(fp);
		return NULL;
	};
	
	png_infop info_ptr = png_create_info_struct(png_ptr);
	if (!info_ptr)
	{
		DDI_ERROR("Failed to create PNG info struct");
		fclose(fp);
		png_destroy_read_struct(&png_ptr, NULL, NULL);
		return NULL;
	};
	
	if (setjmp(png_jmpbuf(png_ptr)))
	{
		DDI_ERROR("Error during png_init_io");
		fclose(fp);
		png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
		return NULL;
	};
	
	png_init_io(png_ptr, fp);
	png_set_sig_bytes(png_ptr, 8);
	png_read_info(png_ptr, info_ptr);
	
	unsigned int width = png_get_image_width(png_ptr, info_ptr);
	unsigned int height = png_get_image_height(png_ptr, info_ptr);
	png_byte colorType = png_get_color_type(png_ptr, info_ptr);
	
	png_read_update_info(png_ptr, info_ptr);
	
	if (setjmp(png_jmpbuf(png_ptr)))
	{
		DDI_ERROR("Failed to read PNG file");
		fclose(fp);
		png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
		return NULL;
	};
	
	png_bytep *rowPointers = (png_bytep*) malloc(sizeof(png_bytep) * height);
	int y;
	for (y=0; y<height; y++)
	{
		rowPointers[y] = (png_byte*) malloc(png_get_rowbytes(png_ptr, info_ptr));
	};
	
	png_read_image(png_ptr, rowPointers);
	png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
	fclose(fp);
	
	// OK, now translate that into a surface
	DDIPixelFormat format;
	if (colorType == PNG_COLOR_TYPE_RGB)
	{
		format.bpp = 3;
		format.alphaMask = 0;
	}
	else if (colorType == PNG_COLOR_TYPE_RGBA)
	{
		format.bpp = 4;
		format.alphaMask = 0xFF000000;
	}
	else
	{
		DDI_ERROR("Unsupported color type");
		return NULL;
	};
	
	format.redMask = 0x0000FF;
	format.greenMask = 0x00FF00;
	format.blueMask = 0xFF0000;
	
	format.pixelSpacing = 0;
	format.scanlineSpacing = 0;
	
	DDISurface *surface = ddiCreateSurface(&format, width, height, NULL, 0);
	unsigned char *put = surface->data;
	
	for (y=0; y<height; y++)
	{
		memcpy(put, rowPointers[y], format.bpp*width);
		put += format.bpp*width;
	};
	
	return surface;
};

static int ddiGetIndexForMask(uint32_t mask)
{
	if (mask == 0x0000FF) return 0;
	if (mask == 0x00FF00) return 1;
	if (mask == 0xFF0000) return 2;
	return 3;
};

DDISurface* ddiConvertSurface(DDIPixelFormat *format, DDISurface *surface, const char **error)
{
	// find the shuffle mask to use
	DDIByteVector convertMask;
	int indexIntoRed = ddiGetIndexForMask(format->redMask);
	int indexIntoGreen = ddiGetIndexForMask(format->greenMask);
	int indexIntoBlue = ddiGetIndexForMask(format->blueMask);
	int indexIntoAlpha = ddiGetIndexForMask(format->alphaMask);
	
	convertMask[indexIntoRed] = ddiGetIndexForMask(surface->format.redMask);
	convertMask[indexIntoGreen] = ddiGetIndexForMask(surface->format.greenMask);
	convertMask[indexIntoBlue] = ddiGetIndexForMask(surface->format.blueMask);
	convertMask[indexIntoAlpha] = ddiGetIndexForMask(surface->format.alphaMask);
	
	// create the new surface and prepare for conversion
	DDISurface *newsurf = ddiCreateSurface(format, surface->width, surface->height, NULL, 0);
	
	size_t targetPixelSize = format->bpp + format->pixelSpacing;
	
	size_t srcPixelSize = surface->format.bpp + surface->format.pixelSpacing;
	
	unsigned char *put = newsurf->data;
	unsigned char *scan = surface->data;
	
	int x, y;
	for (y=0; y<surface->height; y++)
	{
		for (x=0; x<surface->width; x++)
		{
			*((DDIByteVector*)put) = __builtin_shuffle(*((DDIByteVector*)scan), convertMask);
			
			if (surface->format.alphaMask == 0 && format->alphaMask != 0)
			{
				*((uint32_t*)put) |= format->alphaMask;
			};
			
			put += targetPixelSize;
			scan += srcPixelSize;
		};
		
		put += format->scanlineSpacing;
		scan += surface->format.scanlineSpacing;
	};
	
	return newsurf;
};

DDISurface* ddiLoadAndConvertPNG(DDIPixelFormat *format, const char *filename, const char **error)
{
	DDISurface *org = ddiLoadPNG(filename, error);
	if (org == NULL) return NULL;
	
	DDISurface *surface = ddiConvertSurface(format, org, error);
	ddiDeleteSurface(org);
	return surface;
};

int ddiSavePNG(DDISurface *surface, const char *filename, const char **error)
{
	DDIPixelFormat format;
	format.bpp = 4;
	format.redMask = 0x0000FF;
	format.greenMask = 0x00FF00;
	format.blueMask = 0xFF0000;
	format.alphaMask = 0xFF000000;
	format.pixelSpacing = 0;
	format.scanlineSpacing = 0;
	
	DDISurface *working = ddiConvertSurface(&format, surface, error);
	if (working == NULL) return -1;
	
	FILE *fp = fopen(filename, "wb");
	if (fp == NULL)
	{
		DDI_ERROR("I/O error");
		ddiDeleteSurface(working);
		return -1;
	};
	
	png_structp png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	if (!png_ptr)
	{
		DDI_ERROR("png_create_write_struct failed");
		ddiDeleteSurface(working);
		return -1;
	};
	
	png_infop info_ptr = png_create_info_struct(png_ptr);
	if (!info_ptr)
	{
		DDI_ERROR("png_create_info_struct failed");
		ddiDeleteSurface(working);
		png_destroy_write_struct(&png_ptr, NULL);
		return -1;
	};

	if (setjmp(png_jmpbuf(png_ptr)))
	{
		DDI_ERROR("png_init_io failed");
		ddiDeleteSurface(working);
		png_destroy_write_struct(&png_ptr, &info_ptr);
		return -1;
	};
	
	png_init_io(png_ptr, fp);
	
	if (setjmp(png_jmpbuf(png_ptr)))
	{
		DDI_ERROR("writing failed");
		ddiDeleteSurface(working);
		png_destroy_write_struct(&png_ptr, &info_ptr);
		return -1;
	};
	
	png_set_IHDR(png_ptr, info_ptr, working->width, working->height,
			8, PNG_COLOR_TYPE_RGBA, PNG_INTERLACE_NONE,
			PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);
	
	
	png_write_info(png_ptr, info_ptr);
	
	png_bytep *rowPointers = (png_bytep*) malloc(sizeof(png_bytep) * working->height);
	size_t y;
	for (y=0; y<working->height; y++)
	{
		rowPointers[y] = &working->data[4 * y * working->width];
	};
	
	png_write_image(png_ptr, rowPointers);
	png_write_end(png_ptr, NULL);
	free(rowPointers);
	fclose(fp);
	
	return 0;
};

/**
 * Don't question the use of the "register" keyword here. It causes the function to be approximately
 * 2 times faster, when compiling with GCC.
 */
void ddiBlit(DDISurface *src, int srcX, int srcY, DDISurface *dest, int destX, int destY,
	unsigned int width, unsigned int height)
{
	if (memcmp(&src->format, &dest->format, sizeof(DDIPixelFormat)) != 0)
	{
		return;
	};
	
	if (src->format.alphaMask == 0)
	{
		ddiOverlay(src, srcX, srcY, dest, destX, destY, width, height);
	};
	
	int alphaIndex = ddiGetIndexForMask(src->format.alphaMask);

	// cannot copy from negative source coordinates
	if ((srcX < 0) || (srcY < 0))
	{
		return;
	};
	
	// we can copy to negative target coordinates with some messing around
	while (destX < 0)
	{
		srcX++;
		destX++;
		if (width < 0) return;
		width--;
	};
	
	while (destY < 0)
	{
		srcY++;
		destY++;
		if (height < 0) return;
		height--;
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

static void ddiExpandBitmapRow(unsigned char *put, uint8_t row, const void *fill, int bpp, size_t pixelSize)
{
	static uint8_t mask[8] = {1, 2, 4, 8, 16, 32, 64, 128};
	int i;
	for (i=0; i<8; i++)
	{
		if (row & mask[i])
		{
			memcpy(put, fill, bpp);
		};
		
		put += pixelSize;
	};
};

void ddiExpandBitmap(DDISurface *surface, unsigned int x, unsigned int y, int type, const void *bitmap_, DDIColor *color)
{
	int charHeight;
	switch (type)
	{
	case DDI_BITMAP_8x8:
		charHeight = 8;
		break;
	case DDI_BITMAP_8x16:
		charHeight = 16;
		break;
	default:
		return;
	};
	
	uint32_t pixel;
	ddiColorToPixel(&pixel, &surface->format, color);
	
	if ((x+8) > surface->width)
	{
		return;
	};
	
	if ((y+8) > surface->height)
	{
		return;
	};
	
	const uint8_t *bitmap = (const uint8_t*) bitmap_;
	size_t pixelSize = surface->format.bpp + surface->format.pixelSpacing;
	size_t scanlineSize = surface->format.scanlineSpacing + pixelSize * surface->width;
	
	size_t offset = scanlineSize * y + pixelSize * x;
	unsigned char *put = surface->data + offset;
	
	int by;
	for (by=0; by<charHeight; by++)
	{
		ddiExpandBitmapRow(put, bitmap[by], &pixel, surface->format.bpp, pixelSize);
		put += scanlineSize;
	};
};

long ddiReadUTF8(const char **strptr)
{
	if (**strptr == 0)
	{
		return 0;
	};
	
	if ((**strptr & 0x80) == 0)
	{
		long ret = (long) (uint8_t) **strptr;
		(*strptr)++;
		return ret;
	};
	
	long result = 0;
	uint8_t c = (uint8_t) **strptr;
	(*strptr)++;
	int len = 0;
		
	while (c & 0x80)
	{
		c <<= 1;
		len++;
	};
		
	result = (long) (c >> len);
	
	while (--len)
	{
		c = **strptr;
		(*strptr)++;
		result <<= 6;
		result |= (c & 0x3F);	
	};
	
	return result;
};

DDIFont* ddiLoadFont(const char *family, int size, int style, const char **error)
{
	DDIFont *font = (DDIFont*) malloc(sizeof(DDIFont));
	if (font == NULL)
	{
		DDI_ERROR("Out of memory");
		return NULL;
	};
	
	memset(font, 0, sizeof(DDIFont));
	
	font->size = size;
	if (strlen(family) > 64)
	{
		DDI_ERROR("Font name too long");
		free(font);
		return NULL;
	};
	
	char fontfile[256];
	int type = style & (DDI_STYLE_BOLD | DDI_STYLE_ITALIC);
	
	if (type == 0)
	{
		// regular
		sprintf(fontfile, "/usr/share/fonts/regular/%s.ttf", family);
	}
	else if (type == DDI_STYLE_BOLD)
	{
		sprintf(fontfile, "/usr/share/fonts/bold/%s Bold.ttf", family);
	}
	else if (type == DDI_STYLE_ITALIC)
	{
		sprintf(fontfile, "/usr/share/fonts/italic/%s Italic.ttf", family);
	}
	else
	{
		// bold italic
		sprintf(fontfile, "/usr/share/fonts/bi/%s BoldItalic.ttf", family);
	};
	
	FT_Error fterr = FT_Init_FreeType(&font->lib);
	if (fterr != 0)
	{
		DDI_ERROR("FreeType initialization failed");
		free(font);
		return NULL;
	};
	
	// load the new font
	fterr = FT_New_Face(font->lib, fontfile, 0, &font->face);
	if (fterr != 0)
	{
		DDI_ERROR("Failed to load the font");
		free(font);
		return NULL;
	};
	
	fterr = FT_Set_Char_Size(font->face, 0, size*64, 0, 0);
	if (fterr != 0)
	{
		DDI_ERROR("Failed to set font size");
		FT_Done_Face(font->face);
		FT_Done_FreeType(font->lib);
		free(font);
		return NULL;
	};
	
	return font;
};

DDIPen* ddiCreatePen(DDIPixelFormat *format, DDIFont *font, int x, int y, int width, int height, int scrollX, int scrollY, const char **error)
{
	DDIPen *pen = (DDIPen*) malloc(sizeof(DDIPen));
	memcpy(&pen->format, format, sizeof(DDIPixelFormat));
	pen->font = font;
	pen->x = x;
	pen->y = y;
	pen->width = width;
	pen->height = height;
	pen->wrap = 1;
	pen->align = DDI_ALIGN_LEFT;
	pen->letterSpacing = 0;
	pen->lineHeight = 100;
	pen->cursorPos = -1;
	pen->writePos = 0;
	pen->mask = 0;
	
	DDIColor foreground = {0, 0, 0, 255};
	DDIColor background = {0, 0, 0, 0};
	
	memcpy(&pen->foreground, &foreground, sizeof(DDIColor));
	memcpy(&pen->background, &background, sizeof(DDIColor));

	pen->currentLine = (PenLine*) malloc(sizeof(PenLine));
	pen->currentLine->firstSegment = NULL;
	pen->currentLine->maxHeight = pen->font->face->size->metrics.height/64;
	pen->currentLine->currentWidth = 0;
	pen->currentLine->alignment = DDI_ALIGN_LEFT;
	pen->currentLine->baselineY = 0;
	pen->currentLine->next = NULL;
	pen->firstLine = pen->currentLine;
	
	return pen;
};

void ddiSetPenCursor(DDIPen *pen, int cursorPos)
{
	pen->cursorPos = cursorPos;
};

void ddiDeletePen(DDIPen *pen)
{
	PenLine *line = pen->firstLine;
	PenSegment *seg;
	
	while (line != NULL)
	{
		seg = line->firstSegment;
		while (seg != NULL)
		{
			PenSegment *nextSeg = seg->next;
			if (seg->surface != NULL) ddiDeleteSurface(seg->surface);
			free(seg->widths);
			free(seg);
			seg = nextSeg;
		};
		
		PenLine *nextLine = line->next;
		free(line);
		line = nextLine;
	};
	
	free(pen);
};

void ddiSetPenWrap(DDIPen *pen, int wrap)
{
	pen->wrap = wrap;
};

void ddiSetPenAlignment(DDIPen *pen, int alignment)
{
	pen->align = alignment;
};

void ddiSetPenSpacing(DDIPen *pen, int letterSpacing, int lineHeight)
{
	pen->letterSpacing = letterSpacing;
	pen->lineHeight = lineHeight;
};

DDIGlyphCache* ddiGetGlyph(DDIFont *font, long codepoint)
{
	// first try getting the cache glyph
	DDIGlyphCache *cache;
	for (cache=font->glyphCache[codepoint & 0x1F]; cache!=NULL; cache=cache->next)
	{
		if (cache->codepoint == codepoint) return cache;
	};
	
	FT_UInt glyph = FT_Get_Char_Index(font->face, codepoint);
	FT_Error error = FT_Load_Glyph(font->face, glyph, FT_LOAD_DEFAULT);
	if (error != 0) return NULL;
	error = FT_Render_Glyph(font->face->glyph, FT_RENDER_MODE_NORMAL);
	if (error != 0) return NULL;

	FT_Bitmap *bitmap = &font->face->glyph->bitmap;
	
	cache = (DDIGlyphCache*) malloc(sizeof(DDIGlyphCache));
	cache->next = font->glyphCache[codepoint & 0x1F];
	font->glyphCache[codepoint & 0x1F] = cache;
	cache->codepoint = codepoint;
	cache->bitmap = (uint8_t*) malloc(bitmap->pitch * bitmap->rows);
	memcpy(cache->bitmap, bitmap->buffer, bitmap->pitch * bitmap->rows);
	cache->width = bitmap->width;
	cache->height = bitmap->rows;
	cache->pitch = bitmap->pitch;
	cache->advanceX = font->face->glyph->advance.x;
	cache->advanceY = font->face->glyph->advance.y;
	cache->bitmap_top = font->face->glyph->bitmap_top;
	cache->bitmap_left = font->face->glyph->bitmap_left;
	
	return cache;
};

/**
 * Calculate the size of the surface that will fit the text segment. If the text needs to be wrapped, 'nextEl' will point
 * to the location at which the rendering is to be stopped, and the rest of the text is to be rendered on another line.
 * If a newline character is in the text, the same happens. Otherwise, 'nextEl' is set to NULL.
 * If 'nextEl' is set, the character it points to must be IGNORED.
 */ 
static int calculateSegmentSize(DDIPen *pen, const char *text, int *width, int *height, int *offsetX, int *offsetY, const char **nextEl)
{
	int penX=0, penY=0;
	int minX=0, maxX=0, minY=0, maxY=0;
	
	const char *lastWordEnd = NULL;
	int lastWordWidth = 0;
	int lastWordHeight = 0;
	int lastWordOffX = 0;
	int lastWordOffY = 0;
	
	while (1)
	{
		const char *chptr = text;
		long point = ddiReadUTF8(&text);
		if (point == 0)
		{
			break;
		};
		
		if (pen->mask == 1)
		{
			point = '*';
		}
		else if (pen->mask != 0)
		{
			point = pen->mask;
		};

		if (point == ' ')
		{
			lastWordEnd = chptr;
			lastWordWidth = maxX - minX;
			lastWordHeight = maxY - minY;
			lastWordOffX = 0;
			lastWordOffY = 0;
			
			if (minX < 0) lastWordOffX = -minX;
			if (minY < 0) lastWordOffY = -minY;
		};
		
		if (point == '\n')
		{
			// break the line here
			*nextEl = chptr;
			*width = maxX - minX;
			*height = maxY - minY;
			*offsetX = 0;
			*offsetY = 0;
			
			if (minX < 0) *offsetX = -minX;
			if (minY < 0) *offsetY = -minY;
			return 0;
		};
		
		if (point == '\t')
		{
			penX = penX/DDI_TAB_LEN*DDI_TAB_LEN + DDI_TAB_LEN;
			if (penX > maxX) maxX = penX;
		}
		else
		{
			DDIGlyphCache *glyph = ddiGetGlyph(pen->font, point);
			
			int left = penX + glyph->bitmap_left;
			int top = penY - glyph->bitmap_top;
			if (left < minX) minX = left;
			if (top < minY) minY = top;

			penX += (glyph->advanceX >> 6) + pen->letterSpacing;
			penY += glyph->advanceY >> 6;

			int right = left + (glyph->advanceX >> 6) + pen->letterSpacing;
			int bottom = top + glyph->height;
			if (right > maxX) maxX = right;
			if (bottom > maxY) maxY = bottom;
		};
		
		if ((pen->currentLine->currentWidth + (maxX-minX)) > pen->width)
		{
			if (pen->wrap)
			{
				// we must wrap around at the last word that fit in
				// TODO: mfw nothing was written yet
				*nextEl = lastWordEnd;
				*width = lastWordWidth;
				*height = lastWordHeight;
				*offsetX = lastWordOffX;
				*offsetY = lastWordOffY;
				return 0;
			};
		};
	};
	
	if (minX < 0)
	{
		*offsetX = -minX;
	}
	else
	{
		*offsetX = 0;
	};
	
	if (minY < 0)
	{
		*offsetY = -minY;
	}
	else
	{
		*offsetY = 0;
	};
	
	*width = maxX - minX;
	*height = maxY - minY;
	*nextEl = NULL;
	
	return 0;
};

void ddiWritePen(DDIPen *pen, const char *text)
{
	if (*text == 0) return;
	const char *nextEl;
	int offsetX, offsetY, width, height;
	
	while (1)
	{
		if (calculateSegmentSize(pen, text, &width, &height, &offsetX, &offsetY, &nextEl) != 0)
		{
			break;
		};
		
		// create a segment object for this
		size_t textSize;
		if (nextEl == NULL)
		{
			textSize = strlen(text);
		}
		else
		{
			textSize = nextEl - text;
		};

		// preallocate the width array for the most characters we could possibly have
		// (worst case ASCII)
		int *widths = (int*) malloc(sizeof(int) * (textSize+1));
		size_t numChars = 0;
		
		const char *textEnd = &text[textSize];
		
		DDISurface *surface = ddiCreateSurface(&pen->format, width, height, NULL, 0);
		DDIColor fillColor;
		memcpy(&fillColor, &pen->foreground, sizeof(DDIColor));
		fillColor.alpha = 0;
		ddiFillRect(surface, 0, 0, width, height, &fillColor);

		unsigned int pixelSize = pen->format.bpp + pen->format.pixelSpacing;
		unsigned int scanlineSize = pixelSize * width + pen->format.scanlineSpacing;

		uint32_t pixel;
		ddiColorToPixel(&pixel, &surface->format, &fillColor);
		uint8_t *alphaPtr = (uint8_t*) &pixel + ddiGetIndexForMask(pen->format.alphaMask);
		if (pen->format.alphaMask == 0) alphaPtr = NULL;
		
		if (pen->font->face->size->metrics.height >> 6 > pen->currentLine->maxHeight)
		{
			pen->currentLine->maxHeight = pen->font->face->size->metrics.height >> 6;
		};
		
		int penX = 0, penY = 0;
		int firstCharPos = pen->writePos;
		while (text != textEnd)
		{
			long point = ddiReadUTF8(&text);
			if (pen->mask == 1)
			{
				point = '*';
			}
			else if (pen->mask != 0)
			{
				point = pen->mask;
			};

			if (point == '\t')
			{
				int oldPenX = penX;
				penX = penX/DDI_TAB_LEN*DDI_TAB_LEN + DDI_TAB_LEN;

				pen->writePos++;
				widths[numChars++] = penX - oldPenX;
			}
			else
			{
				DDIGlyphCache *glyph = ddiGetGlyph(pen->font, point);
				if (glyph == NULL) break;
				
				pen->writePos++;
				
				int x, y;
				int putX, putY;
				for (y=0; y<glyph->height; y++)
				{
					for (x=0; x<glyph->width; x++)
					{
						if (glyph->bitmap[y * glyph->pitch + x] != 0)
						{
							putX = penX+offsetX+x+glyph->bitmap_left;
							putY = penY+offsetY+y-glyph->bitmap_top;
							
							if (putX >= 0 && putX < width && putY >= 0 && putY < height)
							{
								if (alphaPtr != NULL) *alphaPtr = glyph->bitmap[y * glyph->pitch + x];
								*((uint32_t*)(surface->data
									+ putY * scanlineSize
									+ putX * pixelSize
								)) = pixel;
							};
						};
					};
				};
			
				widths[numChars++] = (glyph->advanceX >> 6) + pen->letterSpacing;
			
				penX += (glyph->advanceX >> 6) + pen->letterSpacing;
				penY += glyph->advanceY >> 6;
			};
		};

		pen->currentLine->currentWidth += width;
		
		PenSegment *seg = (PenSegment*) malloc(sizeof(PenSegment));
		seg->surface = surface;
		seg->next = NULL;
		seg->numChars = numChars;
		seg->widths = widths;
		seg->firstCharPos = firstCharPos;
		seg->baselineY = offsetY;
		memcpy(&seg->background, &pen->background, sizeof(DDIColor));
		
		if ((-pen->font->face->size->metrics.descender/64) > pen->currentLine->baselineY)
		{
			pen->currentLine->baselineY = -pen->font->face->size->metrics.descender / 64;
		};
		
		// add the segment to the end of the list for the current line
		if (pen->currentLine->firstSegment == NULL)
		{
			pen->currentLine->firstSegment = seg;
		}
		else
		{
			PenSegment *last = pen->currentLine->firstSegment;
			while (last->next != NULL) last = last->next;
			last->next = seg;
		};
		
		pen->currentLine->alignment = pen->align;
		pen->currentLine->lineHeight = pen->lineHeight;
		
		if (nextEl == NULL)
		{
			// we are done
			break;
		}
		else
		{
			widths[numChars++] = 0;
			pen->writePos++;
			seg->widths = widths;
			seg->numChars = numChars;
			
			text = nextEl+1;
			PenLine *line = (PenLine*) malloc(sizeof(PenLine));
			line->firstSegment = NULL;
			line->maxHeight = pen->font->face->size->metrics.height/64;
			line->currentWidth = 0;
			line->alignment = pen->align;
			line->baselineY = 0;
			line->next = NULL;
			pen->currentLine->next = line;
			pen->currentLine = line;
		};
	};
};

void ddiExecutePen2(DDIPen *pen, DDISurface *surface, int flags)
{
	PenLine *line;
	PenSegment *seg;

	PenSegment *dseg = (PenSegment*) malloc(sizeof(PenSegment));
	dseg->surface = NULL;
	dseg->next = NULL;
	dseg->numChars = 1;
	dseg->widths = (int*) malloc(sizeof(int));
	dseg->widths[0] = 0;
	dseg->firstCharPos = pen->writePos;
	dseg->baselineY = 0;
	memcpy(&dseg->background, &pen->background, sizeof(DDIColor));

	if (pen->currentLine->firstSegment == NULL)
	{
		pen->currentLine->firstSegment = dseg;
	}
	else
	{
		PenSegment *last = pen->currentLine->firstSegment;
		while (last->next != NULL) last = last->next;
		last->next = dseg;
	};

	static DDIColor cursorColor = {0, 0, 0, 0xFF};
	
	int drawY = pen->y;
	int lastDrawY = drawY;
	if (flags & DDI_POSITION_BASELINE) lastDrawY -= 12;
	for (line=pen->firstLine; line!=NULL; line=line->next)
	{
		line->drawY = drawY;

		int drawX;
		if (line->alignment == DDI_ALIGN_LEFT)
		{
			drawX = pen->x;
		}
		else if (line->alignment == DDI_ALIGN_CENTER)
		{
			drawX = pen->x + (pen->width / 2) - (line->currentWidth / 2);
		}
		else
		{
			drawX = pen->x + pen->width - line->currentWidth;
		};

		lastDrawY = drawY;
		int baselineY = drawY + line->maxHeight - line->baselineY;
		if (flags & DDI_POSITION_BASELINE) baselineY = drawY;
		for (seg=line->firstSegment; seg!=NULL; seg=seg->next)
		{
			int plotY = baselineY - seg->baselineY;
			lastDrawY = plotY;
			seg->drawX = drawX;
			seg->drawY = plotY;
			
			if (seg->background.alpha != 0)
			{
				if (seg->surface != NULL)
				{
					ddiFillRect(surface, drawX, drawY, seg->surface->width, line->maxHeight, &seg->background);
				};
			};
			
			if (seg->surface != NULL)
			{
				ddiBlit(seg->surface, 0, 0, surface, drawX, plotY, seg->surface->width, seg->surface->height);
			};
			
			if ((pen->cursorPos >= seg->firstCharPos) && (pen->cursorPos < (seg->firstCharPos + seg->numChars)))
			{
				int pos = pen->cursorPos - seg->firstCharPos;
				int cursorX = 0;
				
				int i;
				for (i=0; i<pos; i++)
				{
					cursorX += seg->widths[i];
				};
				
				ddiFillRect(surface, drawX + cursorX, drawY, 1, line->maxHeight, &cursorColor);
			};
			if (seg->surface != NULL) drawX += seg->surface->width;
		};
		
		drawY += line->maxHeight * line->lineHeight / 100;
	};
};

void ddiExecutePen(DDIPen *pen, DDISurface *surface)
{
	ddiExecutePen2(pen, surface, 0);
};

void ddiSetPenBackground(DDIPen *pen, DDIColor *bg)
{
	memcpy(&pen->background, bg, sizeof(DDIColor));
};

void ddiSetPenColor(DDIPen *pen, DDIColor *fg)
{
	memcpy(&pen->foreground, fg, sizeof(DDIColor));
};

void ddiGetPenSize(DDIPen *pen, int *widthOut, int *heightOut)
{
	PenLine *line;
	
	int height = 0;
	int width = 0;
	for (line=pen->firstLine; line!=NULL; line=line->next)
	{
		if (line->currentWidth > width) width = line->currentWidth;
		height += line->maxHeight * line->lineHeight / 100;
	};
	
	*widthOut = width;
	*heightOut = height;
};

void ddiSetPenPosition(DDIPen *pen, int x, int y)
{
	pen->x = x;
	pen->y = y;
};

int ddiPenCoordsToPos(DDIPen *pen, int x, int y)
{
	if ((x < 0) || (y < 0))
	{
		return -1;
	};
	
	PenLine *line;
	PenSegment *seg;

	for (line=pen->firstLine; line!=NULL; line=line->next)
	{
		if ((y < line->drawY) || (y >= (line->drawY+line->maxHeight)))
		{
			if (line->next != NULL)
			{
				continue;
			};
		};
		
		if (line->firstSegment != NULL)
		{
			if (x < line->firstSegment->drawX)
			{
				return line->firstSegment->firstCharPos;
			};
		};
		
		int bestBet = pen->writePos;
		for (seg=line->firstSegment; seg!=NULL; seg=seg->next)
		{
			if (seg->surface != NULL)
			{
				int endX = seg->drawX + seg->surface->width;
				bestBet = seg->firstCharPos + (int)seg->numChars;
			
				if ((x >= seg->drawX) && (x < endX))
				{
					// it's in this segment!
					int offX = x - seg->drawX;
				
					int i;
					for (i=0; seg->widths[i]<offX; i++)
					{
						if ((size_t)i == seg->numChars)
						{
							break;
						};
					
						offX -= seg->widths[i];
					};
				
					if ((seg->widths[i]/2 <= offX) && ((size_t)i != seg->numChars))
					{
						i++;
					};
				
					return seg->firstCharPos + i;
				};
			};
		};
		
		return bestBet;
	};
	
	return 0;
};

void ddiPenSetMask(DDIPen *pen, long mask)
{
	pen->mask = mask;
};

DDISurface* ddiRenderText(DDIPixelFormat *format, DDIFont *font, const char *text, const char **error)
{
	DDIPen *pen = ddiCreatePen(format, font, 0, 0, 100, 100, 0, 0, error);
	if (pen == NULL) return NULL;
	
	ddiSetPenWrap(pen, 0);
	ddiWritePen(pen, text);
	
	int width, height;
	ddiGetPenSize(pen, &width, &height);

	DDI_ERROR("Failed to create surface");
	DDISurface *surface = ddiCreateSurface(format, width, height, NULL, 0);
	if (surface != NULL)
	{
		static DDIColor transparent = {0, 0, 0, 0};
		ddiFillRect(surface, 0, 0, width, height, &transparent);
		ddiExecutePen(pen, surface);
	};
	ddiDeletePen(pen);
	
	return surface;
};

typedef struct
{
	const char *name;
	uint8_t red, green, blue;
} DDIColorName;

int ddiParseColor(const char *str, DDIColor *out)
{
	static DDIColorName colors[] = {
		{"black",	0x00, 0x00, 0x00},
		{"white",	0xFF, 0xFF, 0xFF},
		{"red",		0xFF, 0x00, 0x00},
		{"green",	0x00, 0xFF, 0x00},
		{"blue",	0x00, 0x00, 0xFF},
		{"yellow",	0xFF, 0xFF, 0x00},
		{"cyan",	0x00, 0xFF, 0xFF},
		{"magenta",	0xFF, 0x00, 0xFF},
		{"gray",	0x7F, 0x7F, 0x7F},
		{"pink",	0xFF, 0x7F, 0xFF},
		{"orange",	0x7F, 0x7F, 0x00},
		{NULL, 0, 0, 0}
	};
	
	out->alpha = 255;
	
	if (str[0] == '#')
	{
		if (strlen(str) == 4)
		{
			uint8_t r, g, b;
			if (sscanf(str, "#%1hhx%1hhx%1hhx", &r, &g, &b) != 3)
			{
				return -1;
			};
			
			out->red = (r << 4) | r;
			out->green = (g << 4) | g;
			out->blue = (b << 4) | b;
			return 0;
		}
		else if (strlen(str) == 7)
		{
			if (sscanf(str, "#%2hhx%2hhx%2hhx", &out->red, &out->green, &out->blue) != 3)
			{
				return -1;
			};
			
			return 0;
		}
		else
		{
			return -1;
		};
	}
	else
	{
		DDIColorName *name;
		for (name=colors; name->name!=NULL; name++)
		{
			if (strcmp(name->name, str) == 0)
			{
				out->red = name->red;
				out->green = name->green;
				out->blue = name->blue;
				return 0;
			};
		};
		
		return -1;
	};
};

void ddiColorToString(const DDIColor *col, char *buffer)
{
	sprintf(buffer, "#%02hhx%02hhx%02hhx", col->red, col->green, col->blue);
};

static DDISurface* ddiScaleLinear(DDISurface *surface, unsigned int newWidth, unsigned int newHeight)
{
	DDISurface *out = ddiCreateSurface(&surface->format, newWidth, newHeight, NULL, 0);
	if (out == NULL) return NULL;
	
	unsigned int x, y;
	for (y=0; y<newHeight; y++)
	{
		uint8_t *row = &out->data[((out->format.bpp + out->format.pixelSpacing) * newWidth + out->format.scanlineSpacing) * y];
		
		for (x=0; x<newWidth; x++)
		{
			unsigned int srcX = x * surface->width / newWidth;
			unsigned int srcY = y * surface->height / newHeight;
			
			uint8_t *src = &surface->data[((surface->format.bpp + surface->format.pixelSpacing) * surface->width + surface->format.scanlineSpacing) * srcY + (surface->format.bpp + surface->format.pixelSpacing) * srcX];
			
			memcpy(&row[(out->format.bpp + out->format.pixelSpacing) * x], src, out->format.bpp);
		};
	};
	
	return out;
};

DDISurface* ddiScale(DDISurface *surface, unsigned int newWidth, unsigned int newHeight, int algorithm)
{
	switch (algorithm)
	{
	case DDI_SCALE_BEST:
	case DDI_SCALE_FASTEST:
	case DDI_SCALE_LINEAR:
		return ddiScaleLinear(surface, newWidth, newHeight);
	default:
		return NULL;
	};
};
