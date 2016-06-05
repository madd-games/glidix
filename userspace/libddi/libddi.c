/*
	Glidix Display Device Interface (DDI)

	Copyright (c) 2014-2016, Madd Games.
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
#include <stdio.h>
#include <png.h>
#include <ft2build.h>
#include FT_FREETYPE_H

#include "libddi.h"

typedef uint8_t	DDIByteVector __attribute__ ((vector_size(4)));
typedef int DDIIntVector __attribute__ ((vector_size(16)));

/**
 * The default font (8x8).
 */
static char ddiDefaultFont[128][8] = {
	{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},   // U+0000 (nul)
	{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},   // U+0001
	{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},   // U+0002
	{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},   // U+0003
	{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},   // U+0004
	{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},   // U+0005
	{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},   // U+0006
	{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},   // U+0007
	{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},   // U+0008
	{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},   // U+0009
	{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},   // U+000A
	{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},   // U+000B
	{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},   // U+000C
	{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},   // U+000D
	{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},   // U+000E
	{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},   // U+000F
	{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},   // U+0010
	{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},   // U+0011
	{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},   // U+0012
	{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},   // U+0013
	{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},   // U+0014
	{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},   // U+0015
	{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},   // U+0016
	{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},   // U+0017
	{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},   // U+0018
	{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},   // U+0019
	{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},   // U+001A
	{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},   // U+001B
	{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},   // U+001C
	{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},   // U+001D
	{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},   // U+001E
	{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},   // U+001F
	{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},   // U+0020 (space)
	{ 0x18, 0x3C, 0x3C, 0x18, 0x18, 0x00, 0x18, 0x00},   // U+0021 (!)
	{ 0x36, 0x36, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},   // U+0022 (")
	{ 0x36, 0x36, 0x7F, 0x36, 0x7F, 0x36, 0x36, 0x00},   // U+0023 (#)
	{ 0x0C, 0x3E, 0x03, 0x1E, 0x30, 0x1F, 0x0C, 0x00},   // U+0024 ($)
	{ 0x00, 0x63, 0x33, 0x18, 0x0C, 0x66, 0x63, 0x00},   // U+0025 (%)
	{ 0x1C, 0x36, 0x1C, 0x6E, 0x3B, 0x33, 0x6E, 0x00},   // U+0026 (&)
	{ 0x06, 0x06, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00},   // U+0027 (')
	{ 0x18, 0x0C, 0x06, 0x06, 0x06, 0x0C, 0x18, 0x00},   // U+0028 (()
	{ 0x06, 0x0C, 0x18, 0x18, 0x18, 0x0C, 0x06, 0x00},   // U+0029 ())
	{ 0x00, 0x66, 0x3C, 0xFF, 0x3C, 0x66, 0x00, 0x00},   // U+002A (*)
	{ 0x00, 0x0C, 0x0C, 0x3F, 0x0C, 0x0C, 0x00, 0x00},   // U+002B (+)
	{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x0C, 0x0C, 0x06},   // U+002C (,)
	{ 0x00, 0x00, 0x00, 0x3F, 0x00, 0x00, 0x00, 0x00},   // U+002D (-)
	{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x0C, 0x0C, 0x00},   // U+002E (.)
	{ 0x60, 0x30, 0x18, 0x0C, 0x06, 0x03, 0x01, 0x00},   // U+002F (/)
	{ 0x3E, 0x63, 0x73, 0x7B, 0x6F, 0x67, 0x3E, 0x00},   // U+0030 (0)
	{ 0x0C, 0x0E, 0x0C, 0x0C, 0x0C, 0x0C, 0x3F, 0x00},   // U+0031 (1)
	{ 0x1E, 0x33, 0x30, 0x1C, 0x06, 0x33, 0x3F, 0x00},   // U+0032 (2)
	{ 0x1E, 0x33, 0x30, 0x1C, 0x30, 0x33, 0x1E, 0x00},   // U+0033 (3)
	{ 0x38, 0x3C, 0x36, 0x33, 0x7F, 0x30, 0x78, 0x00},   // U+0034 (4)
	{ 0x3F, 0x03, 0x1F, 0x30, 0x30, 0x33, 0x1E, 0x00},   // U+0035 (5)
	{ 0x1C, 0x06, 0x03, 0x1F, 0x33, 0x33, 0x1E, 0x00},   // U+0036 (6)
	{ 0x3F, 0x33, 0x30, 0x18, 0x0C, 0x0C, 0x0C, 0x00},   // U+0037 (7)
	{ 0x1E, 0x33, 0x33, 0x1E, 0x33, 0x33, 0x1E, 0x00},   // U+0038 (8)
	{ 0x1E, 0x33, 0x33, 0x3E, 0x30, 0x18, 0x0E, 0x00},   // U+0039 (9)
	{ 0x00, 0x0C, 0x0C, 0x00, 0x00, 0x0C, 0x0C, 0x00},   // U+003A (:)
	{ 0x00, 0x0C, 0x0C, 0x00, 0x00, 0x0C, 0x0C, 0x06},   // U+003B (//)
	{ 0x18, 0x0C, 0x06, 0x03, 0x06, 0x0C, 0x18, 0x00},   // U+003C (<)
	{ 0x00, 0x00, 0x3F, 0x00, 0x00, 0x3F, 0x00, 0x00},   // U+003D (=)
	{ 0x06, 0x0C, 0x18, 0x30, 0x18, 0x0C, 0x06, 0x00},   // U+003E (>)
	{ 0x1E, 0x33, 0x30, 0x18, 0x0C, 0x00, 0x0C, 0x00},   // U+003F (?)
	{ 0x3E, 0x63, 0x7B, 0x7B, 0x7B, 0x03, 0x1E, 0x00},   // U+0040 (@)
	{ 0x0C, 0x1E, 0x33, 0x33, 0x3F, 0x33, 0x33, 0x00},   // U+0041 (A)
	{ 0x3F, 0x66, 0x66, 0x3E, 0x66, 0x66, 0x3F, 0x00},   // U+0042 (B)
	{ 0x3C, 0x66, 0x03, 0x03, 0x03, 0x66, 0x3C, 0x00},   // U+0043 (C)
	{ 0x1F, 0x36, 0x66, 0x66, 0x66, 0x36, 0x1F, 0x00},   // U+0044 (D)
	{ 0x7F, 0x46, 0x16, 0x1E, 0x16, 0x46, 0x7F, 0x00},   // U+0045 (E)
	{ 0x7F, 0x46, 0x16, 0x1E, 0x16, 0x06, 0x0F, 0x00},   // U+0046 (F)
	{ 0x3C, 0x66, 0x03, 0x03, 0x73, 0x66, 0x7C, 0x00},   // U+0047 (G)
	{ 0x33, 0x33, 0x33, 0x3F, 0x33, 0x33, 0x33, 0x00},   // U+0048 (H)
	{ 0x1E, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x1E, 0x00},   // U+0049 (I)
	{ 0x78, 0x30, 0x30, 0x30, 0x33, 0x33, 0x1E, 0x00},   // U+004A (J)
	{ 0x67, 0x66, 0x36, 0x1E, 0x36, 0x66, 0x67, 0x00},   // U+004B (K)
	{ 0x0F, 0x06, 0x06, 0x06, 0x46, 0x66, 0x7F, 0x00},   // U+004C (L)
	{ 0x63, 0x77, 0x7F, 0x7F, 0x6B, 0x63, 0x63, 0x00},   // U+004D (M)
	{ 0x63, 0x67, 0x6F, 0x7B, 0x73, 0x63, 0x63, 0x00},   // U+004E (N)
	{ 0x1C, 0x36, 0x63, 0x63, 0x63, 0x36, 0x1C, 0x00},   // U+004F (O)
	{ 0x3F, 0x66, 0x66, 0x3E, 0x06, 0x06, 0x0F, 0x00},   // U+0050 (P)
	{ 0x1E, 0x33, 0x33, 0x33, 0x3B, 0x1E, 0x38, 0x00},   // U+0051 (Q)
	{ 0x3F, 0x66, 0x66, 0x3E, 0x36, 0x66, 0x67, 0x00},   // U+0052 (R)
	{ 0x1E, 0x33, 0x07, 0x0E, 0x38, 0x33, 0x1E, 0x00},   // U+0053 (S)
	{ 0x3F, 0x2D, 0x0C, 0x0C, 0x0C, 0x0C, 0x1E, 0x00},   // U+0054 (T)
	{ 0x33, 0x33, 0x33, 0x33, 0x33, 0x33, 0x3F, 0x00},   // U+0055 (U)
	{ 0x33, 0x33, 0x33, 0x33, 0x33, 0x1E, 0x0C, 0x00},   // U+0056 (V)
	{ 0x63, 0x63, 0x63, 0x6B, 0x7F, 0x77, 0x63, 0x00},   // U+0057 (W)
	{ 0x63, 0x63, 0x36, 0x1C, 0x1C, 0x36, 0x63, 0x00},   // U+0058 (X)
	{ 0x33, 0x33, 0x33, 0x1E, 0x0C, 0x0C, 0x1E, 0x00},   // U+0059 (Y)
	{ 0x7F, 0x63, 0x31, 0x18, 0x4C, 0x66, 0x7F, 0x00},   // U+005A (Z)
	{ 0x1E, 0x06, 0x06, 0x06, 0x06, 0x06, 0x1E, 0x00},   // U+005B ([)
	{ 0x03, 0x06, 0x0C, 0x18, 0x30, 0x60, 0x40, 0x00},   // U+005C (\)
	{ 0x1E, 0x18, 0x18, 0x18, 0x18, 0x18, 0x1E, 0x00},   // U+005D (])
	{ 0x08, 0x1C, 0x36, 0x63, 0x00, 0x00, 0x00, 0x00},   // U+005E (^)
	{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF},   // U+005F (_)
	{ 0x0C, 0x0C, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00},   // U+0060 (`)
	{ 0x00, 0x00, 0x1E, 0x30, 0x3E, 0x33, 0x6E, 0x00},   // U+0061 (a)
	{ 0x07, 0x06, 0x06, 0x3E, 0x66, 0x66, 0x3B, 0x00},   // U+0062 (b)
	{ 0x00, 0x00, 0x1E, 0x33, 0x03, 0x33, 0x1E, 0x00},   // U+0063 (c)
	{ 0x38, 0x30, 0x30, 0x3e, 0x33, 0x33, 0x6E, 0x00},   // U+0064 (d)
	{ 0x00, 0x00, 0x1E, 0x33, 0x3f, 0x03, 0x1E, 0x00},   // U+0065 (e)
	{ 0x1C, 0x36, 0x06, 0x0f, 0x06, 0x06, 0x0F, 0x00},   // U+0066 (f)
	{ 0x00, 0x00, 0x6E, 0x33, 0x33, 0x3E, 0x30, 0x1F},   // U+0067 (g)
	{ 0x07, 0x06, 0x36, 0x6E, 0x66, 0x66, 0x67, 0x00},   // U+0068 (h)
	{ 0x0C, 0x00, 0x0E, 0x0C, 0x0C, 0x0C, 0x1E, 0x00},   // U+0069 (i)
	{ 0x30, 0x00, 0x30, 0x30, 0x30, 0x33, 0x33, 0x1E},   // U+006A (j)
	{ 0x07, 0x06, 0x66, 0x36, 0x1E, 0x36, 0x67, 0x00},   // U+006B (k)
	{ 0x0E, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x1E, 0x00},   // U+006C (l)
	{ 0x00, 0x00, 0x33, 0x7F, 0x7F, 0x6B, 0x63, 0x00},   // U+006D (m)
	{ 0x00, 0x00, 0x1F, 0x33, 0x33, 0x33, 0x33, 0x00},   // U+006E (n)
	{ 0x00, 0x00, 0x1E, 0x33, 0x33, 0x33, 0x1E, 0x00},   // U+006F (o)
	{ 0x00, 0x00, 0x3B, 0x66, 0x66, 0x3E, 0x06, 0x0F},   // U+0070 (p)
	{ 0x00, 0x00, 0x6E, 0x33, 0x33, 0x3E, 0x30, 0x78},   // U+0071 (q)
	{ 0x00, 0x00, 0x3B, 0x6E, 0x66, 0x06, 0x0F, 0x00},   // U+0072 (r)
	{ 0x00, 0x00, 0x3E, 0x03, 0x1E, 0x30, 0x1F, 0x00},   // U+0073 (s)
	{ 0x08, 0x0C, 0x3E, 0x0C, 0x0C, 0x2C, 0x18, 0x00},   // U+0074 (t)
	{ 0x00, 0x00, 0x33, 0x33, 0x33, 0x33, 0x6E, 0x00},   // U+0075 (u)
	{ 0x00, 0x00, 0x33, 0x33, 0x33, 0x1E, 0x0C, 0x00},   // U+0076 (v)
	{ 0x00, 0x00, 0x63, 0x6B, 0x7F, 0x7F, 0x36, 0x00},   // U+0077 (w)
	{ 0x00, 0x00, 0x63, 0x36, 0x1C, 0x36, 0x63, 0x00},   // U+0078 (x)
	{ 0x00, 0x00, 0x33, 0x33, 0x33, 0x3E, 0x30, 0x1F},   // U+0079 (y)
	{ 0x00, 0x00, 0x3F, 0x19, 0x0C, 0x26, 0x3F, 0x00},   // U+007A (z)
	{ 0x38, 0x0C, 0x0C, 0x07, 0x0C, 0x0C, 0x38, 0x00},   // U+007B ({)
	{ 0x18, 0x18, 0x18, 0x00, 0x18, 0x18, 0x18, 0x00},   // U+007C (|)
	{ 0x07, 0x0C, 0x0C, 0x38, 0x0C, 0x0C, 0x07, 0x00},   // U+007D (})
	{ 0x6E, 0x3B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},   // U+007E (~)
	{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}    // U+007F
};

typedef struct PenSegment_
{
	DDISurface *surface;
	struct PenSegment_ *next;
} PenSegment;

typedef struct PenLine_
{
	PenSegment *firstSegment;
	int currentWidth;
	int maxHeight;
	int alignment;
	int lineHeight;				// %
	struct PenLine_ *next;
} PenLine;

struct DDIPen_
{
	/**
	 * The target surface onto which we will eventually draw.
	 */
	DDISurface *surface;
	
	/**
	 * The bounding box onto which to draw.
	 */
	int x, y, width, height;
	
	/**
	 * The current scroll position.
	 */
	int scrollX, scrollY;
	
	/**
	 * FreeType library handle.
	 */
	FT_Library lib;

	/**
	 * Current font.
	 */
	FT_Face face;
	
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

DDISurface* ddiCreateSurface(DDIPixelFormat *format, unsigned int width, unsigned int height, char *data, unsigned int flags)
{
	DDISurface *surface = (DDISurface*) malloc(sizeof(DDISurface));
	memcpy(&surface->format, format, sizeof(DDIPixelFormat));
	
	surface->width = width;
	surface->height = height;
	surface->flags = flags;
	
	if ((flags & DDI_STATIC_FRAMEBUFFER) == 0)
	{
		surface->data = (uint8_t*) malloc(ddiGetSurfaceDataSize(surface));
		if (data != NULL) memcpy(surface->data, data, ddiGetSurfaceDataSize(surface));
	}
	else
	{
		surface->data = data;
	};
	
	return surface;
};

void ddiDeleteSurface(DDISurface *surface)
{
	if ((surface->flags & DDI_STATIC_FRAMEBUFFER) == 0)
	{
		free(surface->data);
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

static void ddiCopy(void *dest, void *src, uint64_t count)
{
	if(!count){return;}
	while(count >= 8){ *(uint64_t*)dest = *(uint64_t*)src; dest += 8; src += 8; count -= 8; };
	while(count >= 4){ *(uint32_t*)dest = *(uint32_t*)src; dest += 4; src += 4; count -= 4; };
	while(count >= 2){ *(uint16_t*)dest = *(uint16_t*)src; dest += 2; src += 2; count -= 2; };
	while(count >= 1){ *(uint8_t*)dest = *(uint8_t*)src; dest += 1; src += 1; count -= 1; };
};

static void ddiFill(void *dest, void *src, uint64_t unit, uint64_t count)
{
	while (count--)
	{
		ddiCopy(dest, src, unit);
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
	char *put = surface->data + offset;
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
	
	char *scan = src->data + pixelSize * srcX + srcScanlineSize * srcY;
	char *put = dest->data + pixelSize * destX + destScanlineSize * destY;
	
	for (; height; height--)
	{
		ddiCopy(put, scan, pixelSize * width);
		scan += srcScanlineSize;
		put += destScanlineSize;
	};
};

#define	DDI_ERROR(msg)	if (error != NULL) *error = msg

DDISurface* ddiLoadPNG(const char *filename, const char **error)
{
	char header[8];
	
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
	png_byte bitDepth = png_get_color_type(png_ptr, info_ptr);
	
	int numPasses = png_set_interlace_handling(png_ptr);
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
	char *put = surface->data;
	
	for (y=0; y<height; y++)
	{
		ddiCopy(put, rowPointers[y], format.bpp*width);
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
	size_t targetScanlineSize = format->scanlineSpacing + targetPixelSize * surface->width;
	
	size_t srcPixelSize = surface->format.bpp + surface->format.pixelSpacing;
	size_t srcScanlineSize = surface->format.scanlineSpacing + srcPixelSize * surface->width;
	
	char *put = newsurf->data;
	char *scan = surface->data;
	
	int x, y;
	for (y=0; y<surface->height; y++)
	{
		for (x=0; x<surface->width; x++)
		{
			*((DDIByteVector*)put) = __builtin_shuffle(*((DDIByteVector*)scan), convertMask);
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
	size_t pixelSize = src->format.bpp + src->format.pixelSpacing;
	size_t srcScanlineSize = src->format.scanlineSpacing + pixelSize * src->width;
	size_t destScanlineSize = dest->format.scanlineSpacing + pixelSize * dest->width;
	
	uint8_t *scan = (uint8_t*) src->data + pixelSize * srcX + srcScanlineSize * srcY;
	uint8_t *put = (uint8_t*) dest->data + pixelSize * destX + destScanlineSize * destY;
	
	for (; height; height--)
	{
		//ddiCopy(put, scan, pixelSize * width);
		size_t count = width;
		uint8_t *scanStart = scan;
		uint8_t *putStart = put;
		
		while (count--)
		{
			uint16_t srcAlpha = (uint16_t) scan[alphaIndex];
			int i;
			for (i=0; i<src->format.bpp; i++)
			{
				//if (i != alphaIndex)
				//{
					put[i] = (uint8_t) (((uint16_t)put[i] * (255-srcAlpha) + (uint16_t)scan[i] * srcAlpha) >> 8);
				//};
			};
			
			scan += pixelSize;
			put += pixelSize;
		};
		
		scan = scanStart + srcScanlineSize;
		put = putStart + destScanlineSize;
	};
};

static void ddiExpandBitmapRow(char *put, uint8_t row, const void *fill, int bpp, size_t pixelSize)
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
	char *put = surface->data + offset;
	
	int by;
	for (by=0; by<charHeight; by++)
	{
		ddiExpandBitmapRow(put, bitmap[by], &pixel, surface->format.bpp, pixelSize);
		put += scanlineSize;
	};
};

void ddiDrawText(DDISurface *surface, unsigned int x, unsigned int y, const char *text, DDIColor *color, void *font)
{
	static DDIColor black = {0, 0, 0, 255};
	if (color == NULL) color = &black;
	
	unsigned int originalX = x;
	while (*text != 0)
	{
		char c = *text++;
		if (c & 0x80) continue;
		
		if (c == '\n')
		{
			y += 8;
			x = originalX;
		}
		else
		{
			ddiExpandBitmap(surface, x, y, DDI_BITMAP_8x8, ddiDefaultFont[c], color);
			x += 8;
		};
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

DDIPen* ddiCreatePen(DDISurface *surface, int x, int y, int width, int height, int scrollX, int scrollY, const char **error)
{
	DDIPen *pen = (DDIPen*) malloc(sizeof(DDIPen));
	pen->surface = surface;
	pen->x = x;
	pen->y = y;
	pen->width = width;
	pen->height = height;
	pen->wrap = 1;
	pen->align = DDI_ALIGN_LEFT;
	pen->letterSpacing = 0;
	pen->lineHeight = 100;
	
	DDIColor foreground = {0, 0, 0, 255};
	DDIColor background = {0, 0, 0, 0};
	
	memcpy(&pen->foreground, &foreground, sizeof(DDIColor));
	memcpy(&pen->background, &background, sizeof(DDIColor));
	
	FT_Error fterr = FT_Init_FreeType(&pen->lib);
	if (fterr != 0)
	{
		DDI_ERROR("FreeType initialization failed");
		free(pen);
		return NULL;
	};
	
	fterr = FT_New_Face(pen->lib, "/usr/share/fonts/regular/DejaVu Sans.ttf", 0, &pen->face);
	if (fterr != 0)
	{
		DDI_ERROR("Failed to load default font!");
		FT_Done_FreeType(pen->lib);
		free(pen);
		return NULL;
	};
	
	fterr = FT_Set_Char_Size(pen->face, 0, 12*64, 0, 0);
	if (fterr != 0)
	{
		DDI_ERROR("Failed to set font size!");
		FT_Done_FreeType(pen->lib);
		free(pen);
		return NULL;
	};
	
	pen->currentLine = (PenLine*) malloc(sizeof(PenLine));
	pen->currentLine->firstSegment = NULL;
	pen->currentLine->maxHeight = 12;
	pen->currentLine->currentWidth = 0;
	pen->currentLine->next = NULL;
	pen->firstLine = pen->currentLine;
	
	return pen;
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
			ddiDeleteSurface(seg->surface);
			free(seg);
			seg = nextSeg;
		};
		
		PenLine *nextLine = line->next;
		free(line);
		line = nextLine;
	};
	
	FT_Done_FreeType(pen->lib);
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

/**
 * Calculate the size of the surface that will fit the text segment. If the text needs to be wrapped, 'nextEl' will point
 * to the location at which the rendering is to be stopped, and the rest of the text is to be rendered on another line.
 * If a newline character is in the text, the same happens. Otherwise, 'nextEl' is set to NULL.
 * If 'nextEl' is set, the character it points to must be IGNORED.
 */ 
static int calculateSegmentSize(DDIPen *pen, const char *text, int *width, int *height, int *offsetX, int *offsetY, const char **nextEl)
{
	FT_Error error;
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
		
		FT_UInt glyph = FT_Get_Char_Index(pen->face, point);
		error = FT_Load_Glyph(pen->face, glyph, FT_LOAD_DEFAULT);
		if (error != 0)
		{
			return -1;
		};
	
		error = FT_Render_Glyph(pen->face->glyph, FT_RENDER_MODE_NORMAL);
		if (error != 0)
		{
			return -1;
		};
		
		FT_Bitmap *bitmap = &pen->face->glyph->bitmap;
		
		int left = penX + pen->face->glyph->bitmap_left;
		int top = penY - pen->face->glyph->bitmap_top;
		if (left < minX) minX = left;
		if (top < minY) minY = top;
		
		int right = left + bitmap->width;
		int bottom = top + bitmap->rows;
		if (right > maxX) maxX = right;
		if (bottom > maxY) maxY = bottom;
		
		if ((pen->currentLine->currentWidth + (maxX-minX)) > pen->width)
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
		
		penX += (pen->face->glyph->advance.x >> 6) + pen->letterSpacing;
		penY += pen->face->glyph->advance.y >> 6;
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
		
		const char *textEnd = &text[textSize];
		
		DDISurface *surface = ddiCreateSurface(&pen->surface->format, width, height, NULL, 0);
		DDIColor fillColor;
		memcpy(&fillColor, &pen->foreground, sizeof(DDIColor));
		fillColor.alpha = 0;
		ddiFillRect(surface, 0, 0, width, height, &fillColor);
		
		if (height > pen->currentLine->maxHeight)
		{
			pen->currentLine->maxHeight = height;
		};
		
		int penX = 0, penY = 0;
		while (text != textEnd)
		{
			long point = ddiReadUTF8(&text);
			
			FT_UInt glyph = FT_Get_Char_Index(pen->face, point);
			FT_Error error = FT_Load_Glyph(pen->face, glyph, FT_LOAD_DEFAULT);
			if (error != 0) break;
			error = FT_Render_Glyph(pen->face->glyph, FT_RENDER_MODE_NORMAL);
			if (error != 0) break;
			
			FT_Bitmap *bitmap = &pen->face->glyph->bitmap;
			
			int x, y;
			for (x=0; x<bitmap->width; x++)
			{
				for (y=0; y<bitmap->rows; y++)
				{
					fillColor.alpha = bitmap->buffer[y * bitmap->pitch + x];
					ddiFillRect(surface, penX+offsetX+x+pen->face->glyph->bitmap_left, penY+offsetY+y-pen->face->glyph->bitmap_top, 1, 1, &fillColor);
				};
			};
			
			penX += (pen->face->glyph->advance.x >> 6) + pen->letterSpacing;
			penY += pen->face->glyph->advance.y >> 6;
		};
		
		pen->currentLine->currentWidth += width;
		
		DDISurface *finalSurface = ddiCreateSurface(&surface->format, width, height, NULL, 0);
		ddiFillRect(finalSurface, 0, 0, width, height, &pen->background);
		ddiBlit(surface, 0, 0, finalSurface, 0, 0, width, height);
		ddiDeleteSurface(surface);
		
		PenSegment *seg = (PenSegment*) malloc(sizeof(PenSegment));
		seg->surface = finalSurface;
		seg->next = NULL;
		
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
			text = nextEl+1;
			PenLine *line = (PenLine*) malloc(sizeof(PenLine));
			line->firstSegment = NULL;
			line->maxHeight = 12;
			line->currentWidth = 0;
			line->next = NULL;
			pen->currentLine->next = line;
			pen->currentLine = line;
		};
	};
};

void ddiExecutePen(DDIPen *pen)
{
	PenLine *line;
	PenSegment *seg;
	
	int drawY = pen->y;
	for (line=pen->firstLine; line!=NULL; line=line->next)
	{
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
		
		for (seg=line->firstSegment; seg!=NULL; seg=seg->next)
		{
			int plotY = drawY + line->maxHeight - seg->surface->height;
			ddiBlit(seg->surface, 0, 0, pen->surface, drawX, plotY, seg->surface->width, seg->surface->height);
			drawX += seg->surface->width;
		};
		
		drawY += line->maxHeight * line->lineHeight / 100;
	};
};

void ddiSetPenBackground(DDIPen *pen, DDIColor *bg)
{
	memcpy(&pen->background, bg, sizeof(DDIColor));
};

void ddiSetPenColor(DDIPen *pen, DDIColor *fg)
{
	memcpy(&pen->foreground, fg, sizeof(DDIColor));
};

int ddiSetPenFont(DDIPen *pen, const char *family, int size, int style, const char **error)
{
	if (strlen(family) > 64)
	{
		DDI_ERROR("Font name too long");
		return -1;
	};
	
	char fontfile[256];
	int type = style & (DDI_STYLE_BOLD | DDI_STYLE_ITALIC);
	
	if (type == 0)
	{
		// regular
		sprintf(fontfile, "/usr/share/fonts/regular/%s.tff", family);
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
	
	// dispose of the old font.
	FT_Done_Face(pen->face);
	
	// load the new font
	FT_Error fterr = FT_New_Face(pen->lib, fontfile, 0, &pen->face);
	if (fterr != 0)
	{
		DDI_ERROR("Failed to load the font");
		return -1;
	};
	
	fterr = FT_Set_Char_Size(pen->face, 0, size*64, 0, 0);
	if (fterr != 0)
	{
		DDI_ERROR("Failed to set font size");
		FT_Done_Face(pen->face);
		return -1;
	};
	
	return 0;
};
