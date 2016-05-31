#include <stdio.h>
#include <libgwm.h>
#include <ft2build.h>
#include FT_FREETYPE_H

int myEventHandler(GWMEvent *ev, GWMWindow *win)
{
	switch (ev->type)
	{
	case GWM_EVENT_CLOSE:
		return -1;
	default:
		return gwmDefaultHandler(ev, win);
	};
};

int penX=10, penY=25;
FT_Library library;
FT_Face face;
FT_Error error;
DDISurface *canvas;

void drawChar(int ch)
{
	FT_UInt glyph = FT_Get_Char_Index(face, ch);
	error = FT_Load_Glyph(face, glyph, FT_LOAD_DEFAULT);
	if (error != 0)
	{
		printf("Failed to load the glyph\n");
		exit(1);
	};
	//printf("glyph loaded\n");
	
	error = FT_Render_Glyph(face->glyph, FT_RENDER_MODE_NORMAL);
	if (error != 0)
	{
		printf("Failed to render the glyph\n");
		exit(1);
	};
	
	//printf("glyph rendered\n");
	
	FT_Bitmap *bitmap = &face->glyph->bitmap;
	//printf("The glyph is %dx%d\n", bitmap->width, bitmap->rows);

	DDIColor color = {0, 0, 0, 0};
	DDISurface *glyphSurface = ddiCreateSurface(&canvas->format, bitmap->width, bitmap->rows, NULL, 0);
	ddiFillRect(glyphSurface, 0, 0, bitmap->width, bitmap->rows, &color);
	
	int x, y;
	for (x=0; x<bitmap->width; x++)
	{
		for (y=0; y<bitmap->rows; y++)
		{
			color.alpha = bitmap->buffer[y * bitmap->pitch + x];
			ddiFillRect(glyphSurface, x, y, 1, 1, &color);
		};
	};
	
	ddiBlit(glyphSurface, 0, 0, canvas, penX + face->glyph->bitmap_left, penY - face->glyph->bitmap_top, bitmap->width, bitmap->rows);
	penX += face->glyph->advance.x >> 6;
	penY += face->glyph->advance.y >> 6;
};

int calculateSegmentSize(const char *text, int *width, int *height, int *offsetX, int *offsetY)
{
	int penX=0, penY=0;
	int minX=0, maxX=0, minY=0, maxY=0;
	
	while (1)
	{
		long point = ddiReadUTF8(&text);
		if (point == 0)
		{
			break;
		};
		
		FT_UInt glyph = FT_Get_Char_Index(face, point);
		error = FT_Load_Glyph(face, glyph, FT_LOAD_DEFAULT);
		if (error != 0)
		{
			printf("Failed to load the glyph\n");
			return -1;
		};
	
		error = FT_Render_Glyph(face->glyph, FT_RENDER_MODE_NORMAL);
		if (error != 0)
		{
			printf("Failed to render the glyph\n");
			return -1;
		};
		
		FT_Bitmap *bitmap = &face->glyph->bitmap;
		
		int left = penX + face->glyph->bitmap_left;
		int top = penY - face->glyph->bitmap_top;
		if (left < minX) minX = left;
		if (top < minY) minY = top;
		
		int right = left + bitmap->width;
		int bottom = top + bitmap->rows;
		if (right > maxX) maxX = right;
		if (bottom > maxY) maxY = bottom;
		
		penX += face->glyph->advance.x >> 6;
		penY += face->glyph->advance.y >> 6;
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
	
	return 0;
};

int main()
{
	if (gwmInit() != 0)
	{
		fprintf(stderr, "Failed to initialize GWM!\n");
		return 1;
	};

	GWMWindow *win = gwmCreateWindow(NULL, "Hello world", 10, 10, 500, 500, GWM_WINDOW_MKFOCUSED);
	if (win == NULL)
	{
		fprintf(stderr, "Failed to create window!\n");
		return 1;
	};
	
	canvas = gwmGetWindowCanvas(win);
	DDIColor testcolor = {0, 0, 0, 255};
	ddiFillRect(canvas, 0, 10, 8, 3, &testcolor);
	
	error = FT_Init_FreeType(&library);
	if (error != 0)
	{
		printf("FreeType failed to initialize\n");
		return 1;
	};
	
	error = FT_New_Face(library, "/usr/share/fonts/regular/DejaVu Sans.ttf", 0, &face);
	if (error != 0)
	{
		printf("Failed to load font\n");
		return 1;
	};
	
	error = FT_Set_Char_Size(face, 0, 20*64, 0, 0);
	if (error != 0)
	{
		printf("Failed to set size\n");
		return 1;
	};

	drawChar('H');
	drawChar('e');
	drawChar('l');
	drawChar('l');
	drawChar('o');

#if 0
	FT_Done_Face(face);
	
	error = FT_New_Face(library, "/usr/share/fonts/bi/DejaVu Sans BoldItalic.ttf", 0, &face);
	if (error != 0)
	{
		printf("Failed to load second font\n");
		return 1;
	};
	
	error = FT_Set_Char_Size(face, 0, 50*64, 0, 0);
	if (error != 0)
	{
		printf("Failed to set second size\n");
		return 1;
	};
#endif

	drawChar(',');
	drawChar(' ');
	drawChar('w');
	drawChar('o');
	drawChar('r');
	drawChar('l');
	drawChar('d');

	int width, height, offX, offY;
	calculateSegmentSize("Hello, world!", &width, &height, &offX, &offY);
	printf("The string is %dx%d, offset (%d, %d)\n", width, height, offX, offY);

	gwmPostDirty();
	
	gwmSetEventHandler(win, myEventHandler);
	gwmMainLoop();
	gwmQuit();
	return 0;
};

