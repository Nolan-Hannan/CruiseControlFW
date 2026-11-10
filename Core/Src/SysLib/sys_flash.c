/* M95P32, SPI. SOC curve storage. */

#include "sys_flash.h"
#include "sys_gpio.h"
#include "sys_spi.h"
#include "sys_time.h"
#include <stddef.h>

#define FLASH_BUS SYS_SPI_M0

#define OP_WREN 0x06U
#define OP_RDSR 0x05U
#define OP_JDID 0x9FU
#define OP_READ 0x03U
#define OP_PGWR 0x02U
#define OP_PGPR 0x0AU
#define OP_SE   0x20U
#define OP_CE   0xC7U

#define SR_WIP 0x01U
#define SR_WEL 0x02U

#define ID_MANUFACTURER 0x20U
#define ID_FAMILY       0x00U
#define ID_DENSITY      0x16U

#define FLASH_SIZE 0x400000UL
#define PAGE_SIZE  512UL

/* sys_spi times out per call, so long reads are chunked. */
#define READ_CHUNK 2048U

#define READY_TIMEOUT_MS  10U
#define WRITE_TIMEOUT_MS  10U
#define SECTOR_TIMEOUT_MS 10U
#define CHIP_TIMEOUT_MS   50U

static void select(void) {
	sys_gpioSet(SPI1_SCSN_FLASH, LO);
}

static void deselect(void) {
	sys_gpioSet(SPI1_SCSN_FLASH, HI);
}

static void buildHeader(uint8_t *out, uint8_t op, uint32_t addr) {
	out[0] = op;
	out[1] = (uint8_t)(addr >> 16);
	out[2] = (uint8_t)(addr >> 8);
	out[3] = (uint8_t)addr;
}

static uint8_t command(uint8_t op) {
	uint8_t ok;

	select();
	ok = sys_spiWrite(FLASH_BUS, &op, 1U);
	deselect();
	return ok;
}

static uint8_t readStatus(uint8_t *status) {
	uint8_t tx[2] = { OP_RDSR, 0U };
	uint8_t rx[2];
	uint8_t ok;

	select();
	ok = sys_spiTransfer(FLASH_BUS, tx, rx, sizeof(tx));
	deselect();
	if (ok) {
		*status = rx[1];
	}
	return ok;
}

static uint8_t waitReady(uint32_t timeoutMs) {
	sys_timer_t timer = { .timerLengthMs = timeoutMs };

	sys_timerInit(&timer);
	while (sys_flashBusy()) {
		if (sys_checkOutTimerMs(&timer)) {
			return FALSE;
		}
	}
	return TRUE;
}

static uint8_t writeEnable(void) {
	uint8_t status;

	return !sys_flashBusy() && command(OP_WREN) && readStatus(&status) &&
	       (status & SR_WEL) != 0U;
}

static uint8_t eraseStart(uint8_t op, uint32_t addr) {
	uint8_t cmdHeader[4];
	uint8_t ok;

	if (addr >= FLASH_SIZE || !writeEnable()) {
		return FALSE;
	}
	buildHeader(cmdHeader, op, addr);
	select();
	ok = sys_spiWrite(FLASH_BUS, cmdHeader, sizeof(cmdHeader));
	deselect();
	return ok;
}

uint8_t sys_flashInit(void) {
	uint8_t tx[4] = { OP_JDID, 0U, 0U, 0U };
	uint8_t rx[4];
	uint8_t ok;

	deselect();
	if (!waitReady(READY_TIMEOUT_MS)) {
		return FALSE;
	}
	select();
	ok = sys_spiTransfer(FLASH_BUS, tx, rx, sizeof(tx));
	deselect();
	return ok && rx[1] == ID_MANUFACTURER && rx[2] == ID_FAMILY && rx[3] == ID_DENSITY;
}

uint8_t sys_flashBusy(void) {
	uint8_t status;

	return !readStatus(&status) || (status & SR_WIP) != 0U;
}

uint8_t sys_flashRead(uint32_t addr, uint8_t *dst, uint16_t len) {
	uint8_t cmdHeader[4];
	uint8_t ok;

	if (dst == NULL || len == 0U || addr >= FLASH_SIZE || len > FLASH_SIZE - addr ||
	    sys_flashBusy()) {
		return FALSE;
	}
	buildHeader(cmdHeader, OP_READ, addr);
	select();
	ok = sys_spiWrite(FLASH_BUS, cmdHeader, sizeof(cmdHeader));
	while (ok && len > 0U) {
		uint16_t chunkLen = (len > READ_CHUNK) ? READ_CHUNK : len;

		ok = sys_spiRead(FLASH_BUS, dst, chunkLen);
		dst += chunkLen;
		len = (uint16_t)(len - chunkLen);
	}
	deselect();
	return ok;
}

static uint8_t pageStart(uint8_t op, uint32_t addr, const uint8_t *src, uint16_t len) {
	uint8_t cmdHeader[4];
	uint8_t ok;

	if (src == NULL || len == 0U || addr >= FLASH_SIZE ||
	    (addr % PAGE_SIZE) + len > PAGE_SIZE || !writeEnable()) {
		return FALSE;
	}
	buildHeader(cmdHeader, op, addr);
	select();
	ok = sys_spiWrite(FLASH_BUS, cmdHeader, sizeof(cmdHeader)) && sys_spiWrite(FLASH_BUS, src, len);
	deselect();
	return ok;
}

static uint8_t pageBlock(uint8_t op, uint32_t addr, const uint8_t *src, uint16_t len) {
	if (src == NULL || len == 0U) {
		return FALSE;
	}
	while (len > 0U) {
		uint16_t chunkLen = (uint16_t)(PAGE_SIZE - (addr % PAGE_SIZE));

		if (chunkLen > len) {
			chunkLen = len;
		}
		if (!pageStart(op, addr, src, chunkLen) || !waitReady(WRITE_TIMEOUT_MS)) {
			return FALSE;
		}
		addr += chunkLen;
		src += chunkLen;
		len = (uint16_t)(len - chunkLen);
	}
	return TRUE;
}

uint8_t sys_flashWriteStart(uint32_t addr, const uint8_t *src, uint16_t len) {
	return pageStart(OP_PGWR, addr, src, len);
}

uint8_t sys_flashProgramStart(uint32_t addr, const uint8_t *src, uint16_t len) {
	return pageStart(OP_PGPR, addr, src, len);
}

uint8_t sys_flashEraseSectorStart(uint32_t addr) {
	return eraseStart(OP_SE, addr);
}

uint8_t sys_flashEraseChipStart(void) {
	if (!writeEnable()) {
		return FALSE;
	}
	return command(OP_CE);
}

uint8_t sys_flashWriteBlock(uint32_t addr, const uint8_t *src, uint16_t len) {
	return pageBlock(OP_PGWR, addr, src, len);
}

uint8_t sys_flashProgramBlock(uint32_t addr, const uint8_t *src, uint16_t len) {
	return pageBlock(OP_PGPR, addr, src, len);
}

uint8_t sys_flashEraseSectorBlock(uint32_t addr) {
	return sys_flashEraseSectorStart(addr) && waitReady(SECTOR_TIMEOUT_MS);
}

uint8_t sys_flashEraseChipBlock(void) {
	return sys_flashEraseChipStart() && waitReady(CHIP_TIMEOUT_MS);
}
