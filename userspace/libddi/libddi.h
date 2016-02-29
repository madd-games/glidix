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

#ifndef DDI_H_
#define DDI_H_

#include <stdint.h>

/**
 * Surface flags.
 */
#define	DDI_STATIC_FRAMEBUFFER			(1 << 0)

/**
 * Types of expandable bitmaps.
 */
#define	DDI_BITMAP_8x8				0

/**
 * Describes the pixel format of a surface.
 */
typedef struct
{
	/**
	 * BYTES per pixel (1, 2, 3 or 4).
	 */
	int					bpp;
	
	/**
	 * Red, green, blue and alpha masks. For less than 32 bits per pixel,
	 * only the low bits shall be used. If a mask is zero, that means the
	 * component is missing. For example, alpha may not be supported.
	 */
	uint32_t				redMask;
	uint32_t				greenMask;
	uint32_t				blueMask;
	uint32_t				alphaMask;
	
	/**
	 * Number of unused bytes between pixels and scanlines.
	 */
	unsigned int				pixelSpacing;
	unsigned int				scanlineSpacing;
} DDIPixelFormat;

/**
 * Describes a surface.
 */
typedef struct
{
	/**
	 * Format of the data.
	 */
	DDIPixelFormat				format;
	
	/**
	 * Image parameters.
	 */
	unsigned int				width;
	unsigned int				height;
	unsigned int				flags;
	
	/**
	 * Points to the pixel data; may be NULL for hardware surfaces.
	 */
	uint8_t*				data;
} DDISurface;

/**
 * Describes a color.
 */
typedef struct
{
	uint8_t					red;
	uint8_t					green;
	uint8_t					blue;
	uint8_t					alpha;
} DDIColor;

/**
 * Returns the amount of data required to represent an image of a certain size in the given format.
 */
size_t ddiGetFormatDataSize(DDIPixelFormat *format, unsigned int width, unsigned int height);

/**
 * Creates a new surface out of pixel data in the specified format. The possible flags are as
 * follows:
 *
 *	DDI_STATIC_FRAMEBUFFER
 *		Causes the resulting surface to be a software surface actually backed at the
 *		specified location; so blitting to it will overwrite your data etc.
 */
DDISurface* ddiCreateSurface(DDIPixelFormat *format, unsigned int width, unsigned int height, char *data, unsigned int flags);

/**
 * Deletes a surface.
 */
void ddiDeleteSurface(DDISurface *surface);

/**
 * Fills a rectangle of a specified color within a surface. Does not support alpha blending.
 */
void ddiFillRect(DDISurface *surface, int x, int y, unsigned int width, unsigned int height, DDIColor *color);

/**
 * Copies a rectangle from one image onto another image.
 */
void ddiBlit(DDISurface *src, int srcX, int srcY, DDISurface *dest, int destX, int destY,
	unsigned int width, unsigned int height);

/**
 * Similar to ddiBlit() but does not perform alpha blending.
 */
void ddiOverlay(DDISurface *src, int srcX, int srcY, DDISurface *dest, int destX, int destY,
	unsigned int width, unsigned int height);
	
/**
 * Loads an image from a PNG file and creates a DDI surface out of it. The pixel format used is whatever format
 * the PNG file uses. Returns the loaded surface on success, or NULL on error. This might fail if the PNG file
 * does not exist, cannot be read, or is invalid or corrupt. If the @error pointer is not NULL, then an error
 * message is stored in it.
 */
DDISurface* ddiLoadPNG(const char *filename, const char **error);

/**
 * Convert a surface to another pixel format. Returns a new surface, in the given format, upon success. Returns
 * NULL if the surface could not be converted to this format. If error is not NULL, then an error message is
 * stored at that pointer. The old surface is NOT deleted in ANY case.
 */
DDISurface* ddiConvertSurface(DDIPixelFormat *format, DDISurface *surface, const char **error);

/**
 * Load a PNG and convert it to the specified format.
 */
DDISurface* ddiLoadAndConvertPNG(DDIPixelFormat *format, const char *filename, const char **error);

/**
 * Expand a bitmap into a surface. 'type' indicates a type of bitmap (DDI_BITMAP_*). 'color' is the foreground
 * color; the background will be transparent. 'bitmap' points to the actual bitmap data. 'surface' specifies
 * the target surface, and (x, y) specifies the coordinates where the bitmap is to be expanded.
 */
void ddiExpandBitmap(DDISurface *surface, unsigned int x, unsigned int y, int type, const void *bitmap, DDIColor *color);

/**
 * Render text to a surface at the specified position. The font must currently be NULL; in future versions, it
 * will be possible to specify what font to use and NULL will mean the default system font. If the color is NULL,
 * then the default color (currently black) is used.
 */
void ddiDrawText(DDISurface *surface, unsigned int x, unsigned int y, const char *text, DDIColor *color, void *font);

#endif
