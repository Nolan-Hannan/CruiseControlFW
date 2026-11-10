/* CRC32 (zlib) and CRC16 (XMODEM). */

#ifndef SYS_CRC_H
#define SYS_CRC_H

#include "defines.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Start with crc = 0; pass the previous result to continue over more data. */
uint32_t sys_crc32(uint32_t crc, const uint8_t *buf, uint32_t len);
uint16_t sys_crc16Xmodem(uint16_t crc, const uint8_t *buf, uint16_t len);

#ifdef __cplusplus
}
#endif

#endif /* SYS_CRC_H */
