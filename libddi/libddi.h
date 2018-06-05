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

#include	<sys/glidix.h>

/**
 * Surface flags (see ddiCreateSurface() ).
 */
#define	DDI_RENDER_TARGET			(1 << 0)
#define	DDI_SHARED				(1 << 1)

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
 * Display device IOCTLs.
 */
#define	DDI_IOCTL_VIDEO_MODESET			_GLIDIX_IOCTL_ARG(DDIModeRequest, _GLIDIX_IOCTL_INT_VIDEO, 1)
#define	DDI_IOCTL_VIDEO_GETINFO			_GLIDIX_IOCTL_ARG(DDIDisplayInfo, _GLIDIX_IOCTL_INT_VIDEO, 2)

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
 * Pen flags.
 */
#define	DDI_POSITION_BASELINE			(1 << 0)

/**
 * Scaling algorithms.
 */
#define	DDI_SCALE_BEST				0
#define	DDI_SCALE_FASTEST			1
#define	DDI_SCALE_LINEAR			2
#define	DDI_SCALE_BORDERED_GRADIENT		3

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
 * Graphics information structure provided by kernel driver.
 */
typedef struct
{
	/**
	 * Name of the renderer client library, e.g. "softrender".
	 */
	char					renderer[32];
	
	/**
	 * Number of presentable surfaces that may be allocated. Must be at least 1.
	 */
	int					numPresentable;
	
	/**
	 * Bitset of supported features.
	 */
	int					features;
	
	/**
	 * Reserved.
	 */
	int					resv[16];
} DDIDisplayInfo;
extern DDIDisplayInfo ddiDisplayInfo;

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
	int					width;
	int					height;
	unsigned int				flags;
	
	/**
	 * Points to the pixel data; may be NULL for hardware surfaces.
	 */
	uint8_t*				data;
	
	/**
	 * Surface ID, for shared surfaces.
	 */
	uint32_t				id;
	
	/**
	 * Renderer data.
	 */
	void*					drvdata;
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
 * Describes a DDI driver by providing pointers to implementations of functions.
 */
typedef struct
{
	/**
	 * Size of this structure.
	 */
	size_t					size;
	
	/**
	 * Name of the renderer.
	 */
	const char*				renderString;
	
	/**
	 * Initialize the driver. The file descriptor referring to the display device is
	 * passed as an argument (note that it is close-on-exec). The returned value is
	 * passed to other driver functions, and can be used to store driver-specific data,
	 * and is called the "driver context" (drvctx).
	 */
	void* (*init)(int fd);
	
	/**
	 * Create a new surface.  This function must set the 'data' field of the given
	 * surface, allocating a surface with the required format and size (as already
	 * given in the DDISurface structure). It may also set 'drvdata' to driver-specific
	 * metadata for the surface. The 'flags' must be respected:
	 *
	 * DDI_SHARED - the 'id' field must be set to a shared surface ID.
	 * DDI_RENDER_TARGET - it must be possible to use this surface as a color attachment.
	 *
	 * If 'data' is not NULL, it is the source pixel data that is to be placed in the surface,
	 * and is expected to be in the correct pixel format. If NULL, the surface begins with
	 * undefined content.
	 *
	 * Return 0 on success, or -1 on error.
	 */
	int (*createSurface)(void* drvctx, DDISurface* surface, char *data);
	
	/**
	 * Open a shared surface. This function is passed the driver context, and a surface
	 * structure which is completly zeroed out except the 'id' field, which is set to
	 * the ID of the desired surface. This function must set the width, height and format
	 * of the surface, as well as attach it to the pixel data, as well as any driver-specific
	 * information ('drvdata'). It must return 0 on success, or -1 on error, and in this case
	 * set errno appropriately.
	 */
	int (*openSurface)(void* drvctx, DDISurface* surface);
	
	/**
	 * Perform a blit - move the 'width' x 'height' pixel sized block from 'srcX', 'srcY' on the source
	 * surface 'src' onto the position 'destX', 'destY' of the destination surface 'dest'. 'drvctx' is
	 * the driver context returned by init().
	 *
	 * Both surfaces are to have the same pixel format; if not, the result is undefined.
	 *
	 * This function must perform alpha blending of the source pixels onto the destination pixels.
	 * All coordinates are valid. Pixels outside of bounds are read as fully transparent, and writing
	 * is secretly discarded.
	 */
	void (*blit)(void *drvctx, DDISurface *src, int srcX, int srcY, DDISurface *dest, int destX, int destY,
			int width, int height);
	
	/**
	 * Similar to blit(), but no alpha blending is performed - the exact pixel values (including alpha channel)
	 * are moved directly. Pixels outside of bounds are ignored and silently discarded.
	 */
	void (*overlay)(void *drvctx, DDISurface *src, int srcX, int srcY, DDISurface *dest, int destX, int destY,
			int width, int height);
	
	/**
	 * Draw a rectangle of the given color at the specified position. This function does NOT perform alpha blending!
	 * All coordinates are allowed. Pixels written outside of bounds are discarded.
	 */
	void (*rect)(void *drvctx, DDISurface *surf, int x, int y, int width, int height, DDIColor *color);
} DDIDriver;
extern DDIDriver* ddiDriver;

/**
 * Describes a pen that draws text.
 */
typedef struct DDIPen_ DDIPen;

/**
 * Describes a font to be used with a pen.
 */
typedef struct DDIFont_ DDIFont;

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
 *	DDI_RENDER_TARGET
 *		The returned surface may be used as a 3D render target.
 *
 *	DDI_SHARED
 *		The returned surface is shared, and can be accessed by other applications.
 *		The 'id' field of the surface structure may be passed by another application to
 *		ddiOpenSurface() to get access to the surface (as long as they're both using
 *		the same display device). A shared surface created by a normal user can only
 *		be opened by the same user. A shared surface created by root is accessible to
 *		all users on the system.
 */
DDISurface* ddiCreateSurface(DDIPixelFormat *format, unsigned int width, unsigned int height, char *data, unsigned int flags);

/**
 * Open a previously created shared surface. Returns a surface on success, or NULL on error
 * (shared surface does not exist), and in this case sets errno to the error number.
 */
DDISurface* ddiOpenSurface(uint32_t id);

/**
 * Deletes a surface.
 */
void ddiDeleteSurface(DDISurface *surface);

/**
 * Fills a rectangle of a specified color within a surface. Does not support alpha blending.
 */
void ddiFillRect(DDISurface *surface, int x, int y, int width, int height, DDIColor *color);

/**
 * Copies a rectangle from one image onto another image.
 */
void ddiBlit(DDISurface *src, int srcX, int srcY, DDISurface *dest, int destX, int destY, int width, int height);

/**
 * Similar to ddiBlit() but does not perform alpha blending.
 */
void ddiOverlay(DDISurface *src, int srcX, int srcY, DDISurface *dest, int destX, int destY, int width, int height);
	
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
 * Save a surface to a PNG image. Returns 0 on success, -1 on error.
 */
int ddiSavePNG(DDISurface *surface, const char *filename, const char **error);

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
 * Write a unicode codepoint to a UTF-8 string buffer. The passed-in buffer must have a size of 9 bytes - enough to store
 * any character plus the terminator. The buffer is filled with a NUL-terminated string containing a single UTF-8 character
 * (which may be multiple bytes).
 */
void ddiWriteUTF8(char *buffer, long codepoint);

/**
 * Count the number of CHARACTERS (rather than bytes) in a UTF-8 string.
 */
size_t ddiCountUTF8(const char *str);

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
 * Execute a pen, and allow the specification of some flags.
 *
 * DDI_POSITION_BASELINE	Treat Y coordinates of the pen as indicating the baseline, not the top of text.
 */
void ddiExecutePen2(DDIPen *pen, DDISurface *surface, int flags);

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
DDISurface* ddiRenderText(DDIPixelFormat *format, DDIFont *font, DDIColor *color, const char *text, const char **error);

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

/**
 * Scale a surface to the specifies size, using the specified algorithm.
 * DDI_SCALE_BEST			Highest-quality algorithm available.
 * DDI_SCALE_FASTEST			Fastest algorithm available.
 * DDI_SCALE_LINEAR			Linear scaling algorithm.
 * DDI_SCALE_BORDERED_GRADIENT		Bordered gradient scaling.
 * Returns a new surface on success, NULL on error.
 */
DDISurface* ddiScale(DDISurface *surface, unsigned int newWidth, unsigned int newHeight, int algorithm);

/**
 * Given a 32-bit mask, return the byte offset into the 32-bit value.
 */
int ddiGetIndexForMask(uint32_t mask);

/**
 * Convert a color specification to a pixel.
 */
void ddiColorToPixel(uint32_t *pixeldata, DDIPixelFormat *format, DDIColor *color);

/**
 * Sample a linear gradient.
 * If 'factor' is 0.0, 'out' is set to 'a'.
 * If 'factor' is 1.0, 'out' is set to 'b'.
 * Everything inbetween, the colors are blended and result stored in 'out'.
 * NOTE: The alpha channel is blended too!
 */
void ddiSampleLinearGradient(DDIColor *out, float factor, DDIColor *a, DDIColor *b);

/**
 * Get the color at the specified pixel of a surface.
 */
void ddiGetPixelColor(DDISurface *surface, int x, int y, DDIColor *color);

#endif
