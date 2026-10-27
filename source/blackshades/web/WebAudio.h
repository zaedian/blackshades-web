#ifndef WEB_AUDIO_H
#define WEB_AUDIO_H

#include <AL/al.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Loads a RIFF/WAVE file and converts it to 16-bit PCM, because that is the
 * one format guaranteed to be supported by every OpenAL implementation.
 * The returned buffer is allocated with malloc() and must be freed by the
 * caller (the normal alutUnloadWAV path does this).
 */
void web_alutLoadWAVFile(const char *filename, ALenum *format, void **wave,
	unsigned int *size, ALsizei *freq);

#ifdef __cplusplus
}
#endif

#endif
