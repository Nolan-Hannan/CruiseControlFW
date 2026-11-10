/* UART command line. */

#ifndef CLI_H
#define CLI_H

#include "defines.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Set to 0 to compile the CLI out. */
#define CLI_ENABLED 1

/* While a mode is set, CLI_Process calls its step each pass instead of parsing text.
 * The step returns FALSE when finished and the prompt comes back. */
typedef uint8_t (*CLI_ModeStep)(void);

uint8_t CLI_Init(void);
void CLI_Process(void);
void CLI_Print(const char *str);
void CLI_SetMode(CLI_ModeStep step);

#ifdef __cplusplus
}
#endif

#endif /* CLI_H */
