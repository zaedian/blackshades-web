/**> HEADER FILES <**/
#include "Text.h"

void Text::LoadFontTexture(char *fileName)
{
	TGAImageRec	*tempTexture;
	GLuint		type;
	
	//Load Image
	tempTexture = LoadTGA( fileName ); 
	//Is it valid?
	if(tempTexture){
		//Alpha channel?
		if ( tempTexture->bpp == 24 )
			type = GL_RGB;
		else
			type = GL_RGBA;
	
		glPixelStorei( GL_UNPACK_ALIGNMENT, 1 );
	
		glGenTextures( 1, &FontTexture );
		glTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );

		glBindTexture( GL_TEXTURE_2D, FontTexture);
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );

		gluBuild2DMipmaps( GL_TEXTURE_2D, type, tempTexture->sizeX, tempTexture->sizeY, type, GL_UNSIGNED_BYTE, tempTexture->data );
		free( tempTexture->data );
		free( tempTexture );
	}
}

void Text::BuildFont()								// Build Our Font Display List
{
	/* Display lists are not available in WebGL.  glPrint() draws the
	 * glyph quads directly, so there is nothing to build here. */
	base = 0;
}

void Text::glPrint(GLint x, GLint y, char *string, int set, float size, float width, float height)	// Where The Printing Happens
{
	const unsigned char *text = (const unsigned char *)string;
	int i;
	int length;

	if (set>1)
	{
		set=1;
	}
	if (!string)
	{
		return;
	}

	glTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );
	glEnable(GL_TEXTURE_2D);					// Text glyphs are textured quads
	glBindTexture(GL_TEXTURE_2D, FontTexture);			// Select Our Font Texture
	glDisable(GL_DEPTH_TEST);							// Disables Depth Testing
	glDisable(GL_LIGHTING);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
	glMatrixMode(GL_PROJECTION);						// Select The Projection Matrix
	glPushMatrix();										// Store The Projection Matrix
	glLoadIdentity();									// Reset The Projection Matrix
	glOrtho(0,width,0,height,-100,100);						// Set Up An Ortho Screen
	glMatrixMode(GL_MODELVIEW);							// Select The Modelview Matrix
	glPushMatrix();										// Store The Modelview Matrix
	glLoadIdentity();
	glScalef(size,size,1);									// Reset The Modelview Matrix
	glTranslated(x,y,0);								// Position The Text (0,0 - Bottom Left)

	length = (int)strlen(string);

	/* Equivalent of the old per-character display lists: 16x16 cells in a
	 * 16x16 grid, advanced by 10 units per character. */
	glBegin(GL_QUADS);
	for (i = 0; i < length; i++)
	{
		int glyph = (int)text[i] - 32 + (128 * set);
		float cx;
		float cy;
		float x0;

		if (glyph < 0 || glyph >= 256)
		{
			continue;
		}

		cx = (float)(glyph % 16) / 16.0f;
		cy = (float)(glyph / 16) / 16.0f;
		x0 = (float)i * 10.0f;

		glTexCoord2f(cx,1-cy-0.0625f+.001);			// Texture Coord (Bottom Left)
		glVertex2f(x0,0);						// Vertex Coord (Bottom Left)
		glTexCoord2f(cx+0.0625f,1-cy-0.0625f+.001);	// Texture Coord (Bottom Right)
		glVertex2f(x0+16,0);						// Vertex Coord (Bottom Right)
		glTexCoord2f(cx+0.0625f,1-cy-.001);			// Texture Coord (Top Right)
		glVertex2f(x0+16,16);						// Vertex Coord (Top Right)
		glTexCoord2f(cx,1-cy+.001);					// Texture Coord (Top Left)
		glVertex2f(x0,16);						// Vertex Coord (Top Left)
	}
	glEnd();

	glMatrixMode(GL_PROJECTION);						// Select The Projection Matrix
	glPopMatrix();										// Restore The Old Projection Matrix
	glMatrixMode(GL_MODELVIEW);							// Select The Modelview Matrix
	glPopMatrix();										// Restore The Old Projection Matrix
	glEnable(GL_DEPTH_TEST);							// Enables Depth Testing
	glTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );
}
