/*
 * gluBuild2DMipmaps() replacement for Emscripten.
 *
 * Emscripten's libGL emulation provides gluPerspective(), but no texture
 * helpers.  Black Shades only calls gluBuild2DMipmaps() with unsigned-byte
 * RGB/RGBA images whose dimensions are powers of two, so a small box filter
 * is enough to build the complete mip chain in software.
 */

#include <stdlib.h>
#include <string.h>

#include <GL/gl.h>
#include <GL/glu.h>

static int channels_for_format(GLenum format)
{
	switch (format) {
		case GL_RGB:
		case GL_BGR:
			return 3;
		case GL_RGBA:
		case GL_BGRA:
			return 4;
		case GL_LUMINANCE:
		case GL_ALPHA:
			return 1;
		case GL_LUMINANCE_ALPHA:
			return 2;
		default:
			return 0;
	}
}

static unsigned int texel_offset(int x, int y, int width, int channels)
{
	return ((unsigned int)y * (unsigned int)width + (unsigned int)x) *
		(unsigned int)channels;
}

extern "C" GLint GLAPIENTRY gluBuild2DMipmaps(GLenum target,
	GLint internalFormat, GLsizei width, GLsizei height, GLenum format,
	GLenum type, const void *data)
{
	int channels;
	int w = (int)width;
	int h = (int)height;
	int level;
	unsigned char *current;
	const unsigned char *source = (const unsigned char *)data;

	if (!data || width <= 0 || height <= 0 || type != GL_UNSIGNED_BYTE) {
		return -1;
	}

	channels = channels_for_format(format);
	if (channels == 0) {
		/* Unknown layout: upload level zero only and let it be a plain
		 * non-mipmapped texture. */
		glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
		glTexImage2D(target, 0, internalFormat, width, height, 0,
			format, type, data);
		return 0;
	}

	if (w > 1 || h > 1) {
		int is_pot = (w & (w - 1)) == 0 && (h & (h - 1)) == 0;
		if (!is_pot) {
			/* Non-power-of-two: mipmaps are not supported in WebGL 1.
			 * Level zero is still perfectly usable with GL_LINEAR. */
			glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
			glTexImage2D(target, 0, internalFormat, width, height, 0,
				format, type, data);
			return 0;
		}
	}

	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glTexImage2D(target, 0, internalFormat, width, height, 0,
		format, type, data);

	current = (unsigned char *)malloc((size_t)w * h * channels);
	if (!current) {
		return -1;
	}
	memcpy(current, source, (size_t)w * h * channels);

	level = 0;
	while (w > 1 || h > 1) {
		int nw = (w > 1) ? w / 2 : 1;
		int nh = (h > 1) ? h / 2 : 1;
		int x, y, c;
		unsigned char *next = (unsigned char *)malloc(
			(size_t)nw * nh * channels);
		if (!next) {
			free(current);
			return -1;
		}

		for (y = 0; y < nh; y++) {
			for (x = 0; x < nw; x++) {
				int sx = x * 2;
				int sy = y * 2;
				int count = 0;
				unsigned int sum[4] = {0, 0, 0, 0};
				int dy, dx;

				for (dy = 0; dy < 2; dy++) {
					for (dx = 0; dx < 2; dx++) {
						int px = sx + dx;
						int py = sy + dy;
						unsigned int base;
						if (px >= w || py >= h) {
							continue;
						}
						base = texel_offset(px, py, w, channels);
						for (c = 0; c < channels; c++) {
							sum[c] += current[base + (unsigned int)c];
						}
						count++;
					}
				}

				for (c = 0; c < channels && c < 4; c++) {
					next[texel_offset(x, y, nw, channels) +
						(unsigned int)c] =
						(unsigned char)(sum[c] / (unsigned int)count);
				}
			}
		}

		level++;
		glTexImage2D(target, level, internalFormat, nw, nh, 0,
			format, type, next);

		free(current);
		current = next;
		w = nw;
		h = nh;
	}

	free(current);
	return 0;
}
