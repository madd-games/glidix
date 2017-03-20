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

#ifndef DDI_H_
#define DDI_H_

#include <stdint.h>
#include <stdlib.h>

#ifdef __glidix__
#	include	<sys/glidix.h>
#endif

/**
 * Surface flags.
 */
#define	DDI_STATIC_FRAMEBUFFER			(1 << 0)
#ifdef __glidix__
#define	DDI_SHARED				(1 << 1)
#endif

/**
 * Types of expandable bitmaps.
 */
#define	DDI_BITMAP_8x8				0
#define	DDI_BITMAP_8x16				1

/**
 * Text alignments.
 */
#define	DDI_ALIGN_LEFT				0
#define	DDI_ALIGN_CENTER			1
#define	DDI_ALIGN_RIGHT				2

/**
 * Font styles.
 */
#define	DDI_STYLE_REGULAR			0
#define	DDI_STYLE_BOLD				(1 << 0)
#define	DDI_STYLE_ITALIC			(1 << 1)
#define	DDI_STYLE_UNDERLINE			(1 << 2)

/**
 * Length (in pixels) of a tab.
 */
#define	DDI_TAB_LEN				64

/**
 * Display device IOCTLs for Glidix.
 */
#ifdef __glidix__
#define	DDI_IOCTL_VIDEO_MODESET			_GLIDIX_IOCTL_ARG(DDIModeRequest, _GLIDIX_IOCTL_INT_VIDEO, 1)
#define	DDI_IOCTL_VIDEO_GETFLAGS		_GLIDIX_IOCTL_NOARG(_GLIDIX_IOCTL_INT_VIDEO, 2)
#endif

/**
 * Resolution specifications (bottom 8 bits are the setting type).
 */
#define DDI_RES_AUTO				0		/* best resolution for screen (or safe if not detected) */
#define	DDI_RES_SAFE				1		/* safe resolution (any availabe safe resolution, e.g. 720x480) */
#define	DDI_RES_SPECIFIC(w, h)			(2 | ((w) << 8) | ((h) << 32))

/**
 * Macros to detect exact resolution specification, and extract width and height.
 */
#define	DDI_RES_IS_EXACT(s)			(((s) & 0xFF) == 2)
#define	DDI_RES_WIDTH(s)			(((s) >> 8) & 0xFFFF)
#define	DDI_RES_HEIGHT(s)			(((s) >> 32) & 0xFFFF)

/**
 * Size of a color string.
 */
#define	DDI_COLOR_STRING_SIZE			8

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
 * Request for video mode setting (IOCTL).
 */
typedef struct
{
	/**
	 * (In) Requested resolution (see VIDEO_RES_* macros above).
	 * (Out) Actual resolution set (use VIDEO_RES_WIDTH() and VIDEO_RES_HEIGHT() to extract values)
	 */
	uint64_t				res;
	
	/**
	 * (Out) The pixel format of the display. The framebuffer is placed at virtual offset 0.
	 */
	DDIPixelFormat				format;
} DDIModeRequest;

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
	
	/**
	 * Surface ID, for shared surfaces.
	 */
	uint32_t				id;
} DDISurface;

/**
 * Describes a pen that draws text.
 */
typedef struct DDIPen_ DDIPen;

/**
 * Describes a font to be used with a pen.
 */
typedef struct DDIFont_ DDIFont;

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
 * Initialize the DDI library, opening the specified display device, with the specified oflag.
 * The oflag must be O_RDONLY (for normal uses) or O_RDWR (for the GUI).
 *
 * Returns 0 on success, or -1 on error; in which case, 'errno' is set appropriately by the
 * failing system call.
 */
int ddiInit(const char *display, int oflag);

/**
 * Quit the DDI library. This closes the display device.
 */
void ddiQuit();

/**
 * Set the video mode. The 'res' parameter specifies the desired resolution, and shall be defined using
 * the DDI_RES_* macros. Only root is allowed to set video modes, and DDI must be initialized with
 * O_RDWR. Returns a surface representing the screen on success, or NULL on error, in which case 'errno'
 * is set by the failing system call.
 *
 * Only available when compiled for Glidix.
 */
DDISurface* ddiSetVideoMode(uint64_t res);

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
 * Open a previously created shared surface. Returns a surface on success, or NULL on error
 * (shared surface does not exist).
 *
 * Only available when compiled for Glidix.
 */
DDISurface* ddiOpenSurface(uint32_t id);

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
 * Read a unicode codepoint from a UTF-8 string. 'strptr' refers to a UTF-8 string pointer to read from. If it points
 * to the end of a string, 0 is returned and the pointer is not updated. Otherwise, the codepoint number is returned,
 * and the pointer is updated to point to the next character.
 */
long ddiReadUTF8(const char **strptr);

/**
 * Load the specified font.
 */
DDIFont* ddiLoadFont(const char *family, int size, int style, const char **error);

/**
 * Create a new pen object with the specified target surface format, bounding box and scroll position.
 */
DDIPen* ddiCreatePen(DDIPixelFormat *format, DDIFont *font, int x, int y, int width, int height, int scrollX, int scrollY, const char **error);

/**
 * Delete a pen object.
 */
void ddiDeletePen(DDIPen *pen);

/**
 * Set whether the text written by a pen should be wrapped. Default is true.
 */
void ddiSetPenWrap(DDIPen *pen, int wrap);

/**
 * Set the alignment of text written by a pen. Possible values are DDI_ALIGN_LEFT, DDI_ALIGN_CENTER, or DDI_ALIGN_RIGHT.
 * The default value is DDI_ALIGN_LEFT.
 */
void ddiSetPenAlignment(DDIPen *pen, int alignment);

/**
 * Set the letter spacing and line height of the pen object. The letter spacing is given in pixels, while the line height
 * is given as a percentage. 0 and 100 are the defaults, respectively.
 */
void ddiSetPenSpacing(DDIPen *pen, int letterSpacing, int lineHeight);

/**
 * Write some text using the given pen, at the end of the current text. Note that no text is actually rendered to screen
 * until you call ddiExecutePen(). The currently-set font and other parameters are used for this text segment. Note that
 * line height and alignment apply to whole lines; the value of those attributes for the line is decided once the end of
 * it is reached.
 */
void ddiWritePen(DDIPen *pen, const char *text);

/**
 * Execute a pen. This draws all the text to the target surface.
 */
void ddiExecutePen(DDIPen *pen, DDISurface *surface);

/**
 * Set a pen's background color.
 */
void ddiSetPenBackground(DDIPen *pen, DDIColor *bg);

/**
 * Set a pen's foreground color.
 */
void ddiSetPenColor(DDIPen *pen, DDIColor *fg);

/**
 * Get the current width and height required by the pen.
 */
void ddiGetPenSize(DDIPen *pen, int *widthOut, int *heightOut);

/**
 * Reposition the pen.
 */
void ddiSetPenPosition(DDIPen *pen, int x, int y);

/**
 * Set the pen cursor position. -1 means the cursor shouldn't be drawn. Please call this before making any
 * calls to ddiWritePen() on this pen.
 */
void ddiSetPenCursor(DDIPen *pen, int cursorPos);

/**
 * Given a point (x, y) on a surface that the pen was drawn on, returns the unicode-character offset into the
 * original text, specifying the position that is located at that point. Only valid after a call to ddiExecutePen().
 * Returns -1 if no text is at that position.
 */
int ddiPenCoordsToPos(DDIPen *pen, int x, int y);

/**
 * Set the masking character for a pen. 0 means no masking; 1 means default mask; everything else is a Unicode codepoint
 * to be used as the mask.
 */
void ddiPenSetMask(DDIPen *pen, long mask);

/**
 * Renders text to a new surface, as small as possible, and returns that new surface.
 */
DDISurface* ddiRenderText(DDIPixelFormat *format, DDIFont *font, const char *text, const char **error);

/**
 * Parse a color string. This is either a HTML color specification (#RGB or #RRGGBB), or a HTML color name.
 * The resulting color is stored in 'out' on success, and 0 is returned. If the string is invalid, -1 is
 * returned. The alpha channel is always set to 255.
 */
int ddiParseColor(const char *str, DDIColor *out);

/**
 * Convert a color to a string. 'buffer' must have at least size DDI_COLOR_STRING_SIZE. Always succeeds.
 * The alpha channel is ignored.
 */
void ddiColorToString(const DDIColor *col, char *buffer);

#endif
