/* SPI1/SPI2 bus transfers. */

#ifndef SYS_SPI_H
#define SYS_SPI_H

#include "defines.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    SYS_SPI1,
    SYS_SPI2
} sys_spiBus_t;

uint8_t sys_spiWrite(sys_spiBus_t bus, const uint8_t *tx, uint16_t len);
uint8_t sys_spiRead(sys_spiBus_t bus, uint8_t *rx, uint16_t len);
uint8_t sys_spiTransfer(sys_spiBus_t bus, const uint8_t *tx, uint8_t *rx, uint16_t len);

#ifdef __cplusplus
}
#endif

#endif /* SYS_SPI_H */
