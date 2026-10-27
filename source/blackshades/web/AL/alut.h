#ifndef WEB_ALUT_H
#define WEB_ALUT_H

#include <AL/al.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Minimal ALUT replacement. Only the entry points Black Shades uses are
 * provided; OpenAL itself is Emscripten's OpenAL port. */
void alutInit(int *argc, char **argv);
void alutExit(void);

#ifdef __cplusplus
}
#endif

#endif
