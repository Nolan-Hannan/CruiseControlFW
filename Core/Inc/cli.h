/* UART command line. */

#ifndef CLI_H
#define CLI_H

#include "defines.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Set to 0 to compile the CLI out. */
#define CLI_ENABLED 1

uint8_t CLI_Init(void);
void CLI_Process(void);
void CLI_Print(const char *str);

#ifdef __cplusplus
}
#endif

#endif /* CLI_H */
