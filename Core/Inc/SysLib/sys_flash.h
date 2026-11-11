/* M95P32, SPI. SOC curve storage. */

#ifndef SYS_FLASH_H
#define SYS_FLASH_H

#include "defines.h"

#ifdef __cplusplus
extern "C" {
#endif

uint8_t sys_flashInit(void);
uint8_t sys_flashBusy(void);
uint8_t sys_flashRead(uint32_t addr, uint8_t *dst, uint16_t len);

/* Start calls return immediately; poll sys_flashBusy. A write stays inside one page.
 * Write erases and rewrites the whole page. Program only fills blank bytes, so a power
 * cut cannot damage the rest of the page; program each 16-byte word once between erases. */
uint8_t sys_flashWriteStart(uint32_t addr, const uint8_t *src, uint16_t len);
uint8_t sys_flashProgramStart(uint32_t addr, const uint8_t *src, uint16_t len);
uint8_t sys_flashEraseSectorStart(uint32_t addr);
uint8_t sys_flashEraseChipStart(void);

/* Block calls wait for completion; writes may span pages. */
uint8_t sys_flashWriteBlock(uint32_t addr, const uint8_t *src, uint16_t len);
uint8_t sys_flashProgramBlock(uint32_t addr, const uint8_t *src, uint16_t len);
uint8_t sys_flashEraseSectorBlock(uint32_t addr);
uint8_t sys_flashEraseChipBlock(void);

#ifdef __cplusplus
}
#endif

#endif /* SYS_FLASH_H */
