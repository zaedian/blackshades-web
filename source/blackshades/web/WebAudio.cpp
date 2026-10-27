/*
 * Web audio helpers: ALUT init and a small RIFF/WAVE loader.
 *
 * The PC build used ALUT's WAV loader, which is not available in Emscripten's
 * OpenAL port.  This loader parses the chunks itself and converts PCM data to
 * 16-bit so that only AL_FORMAT_MONO16 / AL_FORMAT_STEREO16 are required.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <AL/al.h>
#include <AL/alc.h>

#include "WebAudio.h"

static ALCdevice *g_device = NULL;
static ALCcontext *g_context = NULL;

extern "C" void alutInit(int *argc, char **argv)
{
	(void)argc;
	(void)argv;

	if (g_context) {
		return;
	}

	g_device = alcOpenDevice(NULL);
	if (!g_device) {
		fprintf(stderr, "alutInit: unable to open OpenAL device\n");
		return;
	}

	g_context = alcCreateContext(g_device, NULL);
	if (!g_context) {
		fprintf(stderr, "alutInit: unable to create OpenAL context\n");
		alcCloseDevice(g_device);
		g_device = NULL;
		return;
	}

	if (!alcMakeContextCurrent(g_context)) {
		fprintf(stderr, "alutInit: unable to make OpenAL context current\n");
	}
}

extern "C" void alutExit(void)
{
	if (g_context) {
		alcMakeContextCurrent(NULL);
		alcDestroyContext(g_context);
		g_context = NULL;
	}
	if (g_device) {
		alcCloseDevice(g_device);
		g_device = NULL;
	}
}

static unsigned int read32(const unsigned char *p)
{
	return (unsigned int)p[0] |
		((unsigned int)p[1] << 8) |
		((unsigned int)p[2] << 16) |
		((unsigned int)p[3] << 24);
}

static unsigned int read16(const unsigned char *p)
{
	return (unsigned int)p[0] | ((unsigned int)p[1] << 8);
}

static void fail(const char *filename, ALenum *format, void **wave,
	unsigned int *size, ALsizei *freq)
{
	fprintf(stderr, "WAV: could not load %s\n", filename);

	/* Keep the game alive with a single silent sample. */
	short *silence = (short *)calloc(1, sizeof(short));
	*format = AL_FORMAT_MONO16;
	*wave = silence;
	*size = silence ? sizeof(short) : 0;
	*freq = 44100;
}

extern "C" void web_alutLoadWAVFile(const char *filename, ALenum *format,
	void **wave, unsigned int *size, ALsizei *freq)
{
	FILE *fp;
	long file_size;
	unsigned char *file_data = NULL;
	const unsigned char *fmt = NULL;
	const unsigned char *data = NULL;
	unsigned int data_size = 0;
	unsigned int fmt_size = 0;
	unsigned int channels = 0;
	unsigned int bits = 0;
	unsigned int sample_rate = 0;
	unsigned int pcm_format = 0;
	unsigned int offset;
	unsigned int frames;
	unsigned int samples;
	unsigned int out_bytes;
	short *out;

	if (!format || !wave || !size || !freq) {
		return;
	}

	*wave = NULL;
	*size = 0;
	*freq = 0;
	*format = AL_FORMAT_MONO16;

	fp = fopen(filename, "rb");
	if (!fp) {
		fail(filename, format, wave, size, freq);
		return;
	}

	if (fseek(fp, 0, SEEK_END) != 0) {
		fclose(fp);
		fail(filename, format, wave, size, freq);
		return;
	}
	file_size = ftell(fp);
	if (file_size <= 0 || fseek(fp, 0, SEEK_SET) != 0) {
		fclose(fp);
		fail(filename, format, wave, size, freq);
		return;
	}

	file_data = (unsigned char *)malloc((size_t)file_size);
	if (!file_data) {
		fclose(fp);
		fail(filename, format, wave, size, freq);
		return;
	}

	if (fread(file_data, 1, (size_t)file_size, fp) != (size_t)file_size) {
		free(file_data);
		fclose(fp);
		fail(filename, format, wave, size, freq);
		return;
	}
	fclose(fp);

	if (file_size < 12 ||
		memcmp(file_data, "RIFF", 4) != 0 ||
		memcmp(file_data + 8, "WAVE", 4) != 0) {
		free(file_data);
		fail(filename, format, wave, size, freq);
		return;
	}

	offset = 12;
	while (offset + 8 <= (unsigned int)file_size) {
		const unsigned char *chunk = file_data + offset;
		unsigned int chunk_size = read32(chunk + 4);
		unsigned int payload = offset + 8;

		if (payload + chunk_size > (unsigned int)file_size) {
			chunk_size = (unsigned int)file_size - payload;
		}

		if (memcmp(chunk, "fmt ", 4) == 0) {
			fmt = file_data + payload;
			fmt_size = chunk_size;
		} else if (memcmp(chunk, "data", 4) == 0) {
			data = file_data + payload;
			data_size = chunk_size;
		}

		offset = payload + chunk_size;
		if (chunk_size & 1) {
			offset++;
		}

		if (fmt && data) {
			break;
		}
	}

	if (!fmt || !data || fmt_size < 16) {
		free(file_data);
		fail(filename, format, wave, size, freq);
		return;
	}

	pcm_format = read16(fmt + 0);
	channels = read16(fmt + 2);
	sample_rate = read32(fmt + 4);
	bits = read16(fmt + 14);

	/* WAVE_FORMAT_EXTENSIBLE: sub-format GUID starts with the format tag. */
	if (pcm_format == 0xFFFE && fmt_size >= 26) {
		pcm_format = read16(fmt + 24);
	}

	if (pcm_format != 1 || (channels != 1 && channels != 2) ||
		(bits != 8 && bits != 16) || sample_rate == 0) {
		free(file_data);
		fail(filename, format, wave, size, freq);
		return;
	}

	frames = data_size / (channels * (bits / 8));
	samples = frames * channels;
	out_bytes = samples * sizeof(short);
	out = (short *)malloc(out_bytes ? out_bytes : sizeof(short));
	if (!out) {
		free(file_data);
		fail(filename, format, wave, size, freq);
		return;
	}

	if (bits == 8) {
		unsigned int i;
		for (i = 0; i < samples; i++) {
			out[i] = (short)(((int)data[i] - 128) << 8);
		}
	} else {
		/* WAV is little-endian, which matches wasm. */
		memcpy(out, data, out_bytes);
	}

	*format = (channels == 1) ? AL_FORMAT_MONO16 : AL_FORMAT_STEREO16;
	*wave = out;
	*size = out_bytes;
	*freq = (ALsizei)sample_rate;

	free(file_data);
}
