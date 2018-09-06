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

#include <sys/glidix.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <png.h>
#include <dlfcn.h>
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
	
	// offset position
	int offsetX, offsetY;
	
	// advance
	int advanceX;
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

static int ddiFD;
DDIDisplayInfo ddiDisplayInfo;
DDIDriver* ddiDriver;
static void *libDriver;
static void *ddiDrvCtx;

int ddiInit(const char *display, int oflag)
{
	ddiFD = open(display, oflag | O_CLOEXEC);
	if (ddiFD == -1)
	{
		return -1;
	};
	
	if (ioctl(ddiFD, DDI_IOCTL_VIDEO_GETINFO, &ddiDisplayInfo) != 0)
	{
		int errnum = errno;
		close(ddiFD);
		errno = errnum;
		return -1;
	};
	
	char renderPath[PATH_MAX];
	char renderName[256];
	
	sprintf(renderPath, "/usr/lib/ddidrv/%s.so", ddiDisplayInfo.renderer);
	sprintf(renderName, "ddidrv_%s", ddiDisplayInfo.renderer);
	
	libDriver = dlopen(renderPath, RTLD_LOCAL | RTLD_LAZY);
	if (libDriver == NULL)
	{
		int errnum = errno;
		close(ddiFD);
		errno = errnum;
		return -1;
	};
	
	ddiDriver = (DDIDriver*) dlsym(libDriver, renderName);
	if (ddiDriver == NULL)
	{
		int errnum = errno;
		close(ddiFD);
		dlclose(libDriver);
		errno = errnum;
		return -1;
	};
	
	ddiDrvCtx = ddiDriver->init(ddiFD);
	return 0;
};

void ddiQuit()
{
	close(ddiFD);
	dlclose(libDriver);
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

DDISurface* ddiCreateSurface(DDIPixelFormat *format, unsigned int width, unsigned int height, char *data, unsigned int flags)
{
	DDISurface *surface = (DDISurface*) malloc(sizeof(DDISurface));
	memset(surface, 0, sizeof(DDISurface));
	memcpy(&surface->format, format, sizeof(DDIPixelFormat));
	
	surface->width = width;
	surface->height = height;
	surface->flags = flags;
	
	if (ddiDriver->createSurface(ddiDrvCtx, surface, data) != 0)
	{
		free(surface);
		return NULL;
	};
	
	return surface;
};

DDISurface* ddiOpenSurface(uint32_t id)
{
	DDISurface *surface = (DDISurface*) malloc(sizeof(DDISurface));
	memset(surface, 0, sizeof(DDISurface));
	surface->id = id;

	if (ddiDriver->openSurface(ddiDrvCtx, surface) != 0)
	{
		int errnum = errno;
		free(surface);
		errno = errnum;
		return NULL;
	};
	
	return surface;
};

void ddiDeleteSurface(DDISurface *surface)
{
	munmap(surface->data, ddiGetSurfaceDataSize(surface));
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

void ddiColorToPixel(uint32_t *pixeldata, DDIPixelFormat *format, DDIColor *color)
{
	*pixeldata = 0;
	ddiInsertWithMask(pixeldata, format->redMask, color->red);
	ddiInsertWithMask(pixeldata, format->greenMask, color->green);
	ddiInsertWithMask(pixeldata, format->blueMask, color->blue);
	ddiInsertWithMask(pixeldata, format->alphaMask, color->alpha);
};

void ddiFillRect(DDISurface *surf, int x, int y, int width, int height, DDIColor *color)
{
	ddiDriver->rect(ddiDrvCtx, surf, x, y, width, height, color);
};

void ddiOverlay(DDISurface *src, int srcX, int srcY, DDISurface *dest, int destX, int destY, int width, int height)
{
	ddiDriver->overlay(ddiDrvCtx, src, srcX, srcY, dest, destX, destY, width, height);
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

int ddiGetIndexForMask(uint32_t mask)
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

void ddiBlit(DDISurface *src, int srcX, int srcY, DDISurface *dest, int destX, int destY, int width, int height)
{
	ddiDriver->blit(ddiDrvCtx, src, srcX, srcY, dest, destX, destY, width, height);
};

static void ddiExpandBitmapRow(unsigned char *put, uint8_t row, const void *fill, int bpp, size_t pixelSize)
{
	static uint8_t mask[8] = {128, 64, 32, 16, 8, 4, 2, 1};
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

void ddiWriteUTF8(char *buffer, long codepoint)
{
	if (codepoint < 128)
	{
		*buffer++ = (char) codepoint;
		*buffer = 0;
	}
	else
	{
		uint8_t temp[9];
		uint8_t *put = &temp[8];
		*put = 0;
		
		uint64_t len = 0;
		while (codepoint != 0)
		{
			// calculate the number of bits that we could now encode using only 2 bytes
			uint64_t maxBits = 6 + (6 - (len+1));
			long mask = (1L << maxBits)-1;
			
			if ((codepoint & mask) == codepoint)
			{
				// finishing sequence
				*--put = (uint8_t) ((codepoint & 0x3F) | 0x80);
				codepoint >>= 6;
				
				uint8_t mask = ~((1 << (8-(len+2)))-1);
				*--put = (uint8_t) (codepoint | mask);
				
				break;
			}
			else
			{
				// not yet; encode the next 6 bits
				*--put = (uint8_t) ((codepoint & 0x3F) | 0x80);
				codepoint >>= 6;
				len++;
			};
		};
		
		strcpy(buffer, (char*) put);
	};
};

size_t ddiCountUTF8(const char *str)
{
	size_t out = 0;
	while (ddiReadUTF8(&str) != 0) out++;
	return out;
};

const char* ddiGetOffsetUTF8(const char *str, size_t n)
{
	while (n--)
	{
		ddiReadUTF8(&str);
	};
	
	return str;
};

DDIFont* ddiLoadFont(const char *family, int size, int style, const char **error)
{
	if (strlen(family) > 64)
	{
		DDI_ERROR("Font name too long");
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

	return ddiOpenFont(fontfile, size, error);
};

DDIFont* ddiOpenFont(const char *fontfile, int size, const char **error)
{
	DDIFont *font = (DDIFont*) malloc(sizeof(DDIFont));
	if (font == NULL)
	{
		DDI_ERROR("Out of memory");
		return NULL;
	};
	
	memset(font, 0, sizeof(DDIFont));
	
	font->size = size;
	
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
		fprintf(stderr, "libddi: warning: loading font `%s' failed\n", fontfile);
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

static int isExclusiveBreak(long codepoint)
{
	return codepoint == ' ' || codepoint == '\t';
};

static int isInclusiveBreak(long codepoint)
{
	// TODO: just an example for now; basically all punctuation should be an inclusive breakpoint
	return codepoint == '-' || codepoint == '_' || codepoint == '/' || codepoint == '.' || codepoint == ',';
};

/**
 * Calculate the size of the surface that will fit the text segment. If the text needs to be wrapped, 'nextEl' will point
 * to the location at which the rendering is to be stopped, and the rest of the text is to be rendered on another line.
 * If a newline character is in the text, the same happens. Otherwise, 'nextEl' is set to NULL.
 * If 'nextEl' is set, the character it points to must be IGNORED, UNLESS *include gets set to 1.
 */ 
static int calculateSegmentSize(DDIPen *pen, const char *text, int *width, int *height, int *offsetX, int *offsetY,
		const char **nextEl, int *include, int *advanceX)
{
	int penX=0, penY=0;
	int minX=0, maxX=0, minY=0, maxY=0;
	
	const char *lastWordEnd = NULL;
	int lastWordWidth = 0;
	int lastWordHeight = 0;
	int lastWordOffX = 0;
	int lastWordOffY = 0;
	int includeWord = 0;
	int writtenCharsYet = 0;
	
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
			point = 0x25CF;
		}
		else if (pen->mask != 0)
		{
			point = pen->mask;
		};

		if (isExclusiveBreak(point))
		{
			lastWordEnd = chptr;
			lastWordWidth = maxX - minX;
			lastWordHeight = maxY - minY;
			lastWordOffX = 0;
			lastWordOffY = 0;
			
			if (minX < 0) lastWordOffX = -minX;
			if (minY < 0) lastWordOffY = -minY;
		};

		if (writtenCharsYet && isInclusiveBreak(point))
		{
			lastWordEnd = chptr;
			lastWordWidth = maxX - minX;
			lastWordHeight = maxY - minY;
			lastWordOffX = 0;
			lastWordOffY = 0;
			
			if (minX < 0) lastWordOffX = -minX;
			if (minY < 0) lastWordOffY = -minY;
			
			includeWord = 1;
		};
		
		if (point == '\n')
		{
			// break the line here
			*nextEl = chptr;
			*width = maxX - minX;
			*height = maxY - minY;
			*offsetX = 0;
			*offsetY = 0;
			*advanceX = penX;
			
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
			if (pen->wrap && writtenCharsYet)
			{
				// we must wrap around at the last word that fit in
				if (lastWordEnd == NULL)
				{
					// no words fit, wrap at this character
					*nextEl = chptr;
					*width = maxX - minX;
					*height = maxY - minY;
					*offsetX = 0;
					*offsetY = 0;
					if (minX < 0) *offsetX = -minX;
					if (minY < 0) *offsetY = -minY;
					
					*include = 1;
					*advanceX = penX;
					return 0;
				};
				
				*nextEl = lastWordEnd;
				*width = lastWordWidth;
				*height = lastWordHeight;
				*offsetX = lastWordOffX;
				*offsetY = lastWordOffY;
				*include = includeWord;
				*advanceX = penX;
				return 0;
			};
		};
		
		writtenCharsYet = 1;
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
	*advanceX = penX;
	
	return 0;
};

void ddiWritePen(DDIPen *pen, const char *text)
{
	if (*text == 0) return;
	const char *nextEl;
	int offsetX, offsetY, width, height, advanceX;
	
	while (1)
	{
		int include = 0;
		if (calculateSegmentSize(pen, text, &width, &height, &offsetX, &offsetY, &nextEl, &include, &advanceX) != 0)
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
				point = 0x25CF;
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
		seg->offsetX = offsetX;
		seg->offsetY = offsetY;
		seg->advanceX = advanceX;
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
			if (!include) widths[numChars++] = 0;
			if (!include) pen->writePos++;
			seg->widths = widths;
			seg->numChars = numChars;
			
			text = nextEl+1;
			if (include) text = nextEl;
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
	dseg->offsetX = 0;
	dseg->offsetY = 0;
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
					ddiFillRect(surface, drawX-seg->offsetX, drawY, seg->surface->width, line->maxHeight, &seg->background);
				};
			};
			
			if (seg->surface != NULL)
			{
				ddiBlit(seg->surface, 0, 0, surface, drawX-seg->offsetX, plotY, seg->surface->width, seg->surface->height);
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
			if (seg->surface != NULL) drawX += /*seg->surface->width*/ seg->advanceX;
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

DDISurface* ddiRenderText(DDIPixelFormat *format, DDIFont *font, DDIColor *color, const char *text, const char **error)
{
	DDIPen *pen = ddiCreatePen(format, font, 0, 0, 100, 100, 0, 0, error);
	if (pen == NULL) return NULL;
	
	ddiSetPenWrap(pen, 0);
	ddiSetPenColor(pen, color);
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

static DDISurface* ddiScaleBorderedGradient(DDISurface *surface, unsigned int newWidth, unsigned int newHeight)
{
	DDISurface *out = ddiCreateSurface(&surface->format, newWidth, newHeight, NULL, 0);
	if (out == NULL) return NULL;
	
	ddiOverlay(surface, 0, 0, out, 0, 0, surface->width/2, surface->height/2);
	ddiOverlay(surface, surface->width-surface->width/2, 0, out, newWidth-(surface->width/2), 0, surface->width/2, surface->height/2);
	ddiOverlay(surface, 0, surface->height-surface->height/2, out, 0, newHeight-(surface->height/2), surface->width/2, surface->height/2);
	ddiOverlay(surface, surface->width-surface->width/2, surface->height-surface->height/2, out, newWidth-(surface->width/2), newHeight-(surface->height/2), surface->width/2, surface->height/2);

	if (newWidth < surface->width || newHeight < surface->height) return out;
	
	// top part: mix colors from the left/right borders
	int y;
	for (y=0; y<surface->height/2; y++)
	{
		DDIColor left, right;
		ddiGetPixelColor(surface, surface->width/2, y, &left);
		ddiGetPixelColor(surface, surface->width-surface->width/2, y, &right);
		
		int baseX = surface->width/2-1;
		int endX = newWidth - (surface->width/2);
		int lineWidth = endX - baseX;
		
		int x;
		for (x=1; x<lineWidth; x++)
		{
			float factor = (float) x / (float) lineWidth;
			DDIColor color;
			ddiSampleLinearGradient(&color, factor, &left, &right);
			ddiFillRect(out, baseX+x, y, 1, 1, &color);
		};
	};
	
	// bottom part: same idea
	int baseY = newHeight-(surface->height/2);
	for (y=baseY; y<newHeight; y++)
	{
		int srcY = surface->height - (newHeight - y);
		DDIColor left, right;
		ddiGetPixelColor(surface, surface->width/2, srcY, &left);
		ddiGetPixelColor(surface, surface->width-surface->width/2, srcY, &right);
		
		int baseX = surface->width/2-1;
		int endX = newWidth - (surface->width/2);
		int lineWidth = endX - baseX;
		
		int x;
		for (x=1; x<lineWidth; x++)
		{
			float factor = (float) x / (float) lineWidth;
			DDIColor color;
			ddiSampleLinearGradient(&color, factor, &left, &right);
			ddiFillRect(out, baseX+x, y, 1, 1, &color);
		};
	};
	
	// left part
	int x;
	for (x=0; x<surface->width/2; x++)
	{
		DDIColor top, bottom;
		ddiGetPixelColor(surface, x, surface->height/2, &top);
		ddiGetPixelColor(surface, x, surface->height-surface->height/2, &bottom);
		
		int baseY = surface->height/2-1;
		int endY = newHeight - (surface->height/2);
		int lineHeight = endY - baseY;
		
		int y;
		for (y=1; y<lineHeight; y++)
		{
			float factor = (float) y / (float) lineHeight;
			DDIColor color;
			ddiSampleLinearGradient(&color, factor, &top, &bottom);
			ddiFillRect(out, x, baseY+y, 1, 1, &color);
		};
	};

	// right part
	int baseX = newWidth-(surface->width-surface->width/2);
	for (x=baseX; x<newWidth; x++)
	{
		int srcX = surface->width - (newWidth - x);
		DDIColor top, bottom;
		ddiGetPixelColor(surface, srcX, surface->height/2, &top);
		ddiGetPixelColor(surface, srcX, surface->height-surface->height/2, &bottom);
		
		int baseY = surface->height/2-1;
		int endY = newHeight - (surface->height/2);
		int lineHeight = endY - baseY;
		
		int y;
		for (y=1; y<lineHeight; y++)
		{
			float factor = (float) y / (float) lineHeight;
			DDIColor color;
			ddiSampleLinearGradient(&color, factor, &top, &bottom);
			ddiFillRect(out, x, baseY+y, 1, 1, &color);
		};
	};

	// finally the middle part (quadratic interpolation)
	int subwidth = newWidth - surface->width;
	int subheight = newHeight - surface->height;
	
	baseX = surface->width / 2;
	baseY = surface->height / 2;
	
	unsigned int totalArea = subwidth * subheight;
	
	DDIColor topleft, topright, bottomleft, bottomright;
	ddiGetPixelColor(surface, surface->width/2, baseY, &topleft);
	ddiGetPixelColor(surface, surface->width-surface->width/2, baseY, &topright);
	ddiGetPixelColor(surface, surface->width/2, baseY+1, &bottomleft);
	ddiGetPixelColor(surface, surface->width-surface->width/2, baseY+1, &bottomright);
	
	for (y=0; y<subheight; y++)
	{
		for (x=0; x<subwidth; x++)
		{
			unsigned int areaTL = x * y;
			unsigned int areaTR = (subwidth-x) * y;
			unsigned int areaBL = x * (subheight-y);
			unsigned int areaBR = (subwidth-x) * (subheight-y);
			
			DDIColor result;
			result.red = ((unsigned int) topleft.red * areaTL + (unsigned int) topright.red * areaTR + (unsigned int) bottomleft.red * areaBL + (unsigned int) bottomright.red * areaBR) / totalArea;
			result.green = ((unsigned int) topleft.green * areaTL + (unsigned int) topright.green * areaTR + (unsigned int) bottomleft.green * areaBL + (unsigned int) bottomright.green * areaBR) / totalArea;
			result.blue = ((unsigned int) topleft.blue * areaTL + (unsigned int) topright.blue * areaTR + (unsigned int) bottomleft.blue * areaBL + (unsigned int) bottomright.blue * areaBR) / totalArea;
			result.alpha = ((unsigned int) topleft.alpha * areaTL + (unsigned int) topright.alpha * areaTR + (unsigned int) bottomleft.alpha * areaBL + (unsigned int) bottomright.alpha * areaBR) / totalArea;
			
			ddiFillRect(out, baseX+x, baseY+y, 1, 1, &result);
		};
	};
	
	return out;
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
	case DDI_SCALE_BORDERED_GRADIENT:
		return ddiScaleBorderedGradient(surface, newWidth, newHeight);
	default:
		return NULL;
	};
};

void ddiSampleLinearGradient(DDIColor *out, float fb, DDIColor *a, DDIColor *b)
{
	float fa = 1.0 - fb;
	out->red = a->red * fa + b->red * fb;
	out->green = a->green * fa + b->green * fb;
	out->blue = a->blue * fa + b->blue * fb;
	out->alpha = a->alpha * fa + b->alpha * fb;
};

static int maskToIndex(uint32_t mask)
{
	int index = 0;
	while (mask != 0xFF)
	{
		mask >>= 8;
		index++;
	};
	
	return index;
};

void ddiGetPixelColor(DDISurface *surface, int x, int y, DDIColor *color)
{
	uint8_t *pixel = &surface->data[((surface->format.bpp + surface->format.pixelSpacing) * surface->width + surface->format.scanlineSpacing) * y + (surface->format.bpp + surface->format.pixelSpacing) * x];
	
	if (surface->format.redMask == 0)
	{
		color->red = 0;
	}
	else
	{
		color->red = pixel[maskToIndex(surface->format.redMask)];
	};
	
	if (surface->format.greenMask == 0)
	{
		color->green = 0;
	}
	else
	{
		color->green = pixel[maskToIndex(surface->format.greenMask)];
	};

	if (surface->format.blueMask == 0)
	{
		color->blue = 0;
	}
	else
	{
		color->blue = pixel[maskToIndex(surface->format.blueMask)];
	};

	if (surface->format.alphaMask == 0)
	{
		color->alpha = 0xFF;
	}
	else
	{
		color->alpha = pixel[maskToIndex(surface->format.alphaMask)];
	};
};
