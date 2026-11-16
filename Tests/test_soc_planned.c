/*
 * Host check for sys_socPlanned against a RAM copy of the flash. Not part of the firmware build.
 * From the repo root:
 *   cc -Wall -Wextra -ICore/Inc -ICore/Inc/SysLib Tests/test_soc_planned.c \
 *      Core/Src/SysLib/sys_soc.c Core/Src/SysLib/sys_crc.c -o /tmp/test_soc && /tmp/test_soc
 */

#include "sys_soc.h"
#include "sys_crc.h"
#include "sys_flash.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static uint8_t flash[0xC000];

uint8_t sys_flashRead(uint32_t addr, uint8_t *dst, uint16_t len) {
	memcpy(dst, &flash[addr], len);
	return TRUE;
}

/* Only the download path writes; the lookup never does. */
uint8_t sys_flashWriteBlock(uint32_t addr, const uint8_t *src, uint16_t len) {
	(void)addr; (void)src; (void)len;
	return FALSE;
}
uint8_t sys_flashProgramBlock(uint32_t addr, const uint8_t *src, uint16_t len) {
	(void)addr; (void)src; (void)len;
	return FALSE;
}
uint8_t sys_flashEraseSectorBlock(uint32_t addr) {
	(void)addr;
	return FALSE;
}

static void put16(uint32_t addr, uint16_t v) {
	flash[addr] = (uint8_t)v;
	flash[addr + 1U] = (uint8_t)(v >> 8);
}

int main(void) {
	uint8_t *rec = &flash[EEP_PTR_LOG_ADDR];
	uint8_t *day2 = &flash[EEP_SOC_BANK_B_ADDR + EEP_SOC_DAYHDR_OFF + EEP_SOC_DAYHDR_SIZE];
	const uint32_t data2 = EEP_SOC_BANK_B_ADDR + EEP_SOC_DATA_OFF + EEP_SOC_DAY_STRIDE;
	uint32_t crc;
	world_time_t t = { .day = 14, .month = 7, .year = 2027, .hours = 9, .min = 0, .sec = 0 };
	float soc = 0.0f;

	memset(flash, 0xFF, sizeof(flash));
	assert(!sys_socPlanned(&t, &soc)); /* no curve */

	/* Pointer record naming bank B. */
	memcpy(rec, "BANK", 4U);
	rec[EEP_PTR_BANK_OFF] = EEP_PTR_BANK_B;
	rec[EEP_PTR_INV_OFF] = EEP_PTR_INV_B;
	crc = sys_crc32(0U, rec, EEP_PTR_CRC_OFF);
	memcpy(&rec[EEP_PTR_CRC_OFF], &crc, 4U); /* host is little endian */

	/* Second day: 2027-07-14, 09:00 to 17:00, samples 90.00 %, 80.00 %, then 70.00 % at the end. */
	flash[EEP_SOC_BANK_B_ADDR + EEP_SOC_DAYS_OFF] = EEP_SOC_DAYS;
	day2[EEP_SOC_DAY_MONTH] = 7;
	day2[EEP_SOC_DAY_DAY] = 14;
	day2[EEP_SOC_DAY_YEAR] = (uint8_t)2027;
	day2[EEP_SOC_DAY_YEAR + 1] = (uint8_t)(2027 >> 8);
	day2[EEP_SOC_DAY_START_H] = 9;
	day2[EEP_SOC_DAY_START_M] = 0;
	day2[EEP_SOC_DAY_STOP_H] = 17;
	day2[EEP_SOC_DAY_STOP_M] = 0;
	put16(data2, 9000U);
	put16(data2 + 2U, 8000U);
	put16(data2 + 2U * 2879U, 7000U);

	assert(sys_socPlanned(&t, &soc) && fabsf(soc - 90.0f) < 0.001f);
	t.sec = 5; /* halfway between the first two samples */
	assert(sys_socPlanned(&t, &soc) && fabsf(soc - 85.0f) < 0.001f);
	t.hours = 16; t.min = 59; t.sec = 55; /* last sample, nothing to interpolate to */
	assert(sys_socPlanned(&t, &soc) && fabsf(soc - 70.0f) < 0.001f);

	t.hours = 8; t.min = 59; t.sec = 59; /* before the start */
	assert(!sys_socPlanned(&t, &soc));
	t.hours = 17; t.min = 0; t.sec = 1; /* after the stop */
	assert(!sys_socPlanned(&t, &soc));
	t.hours = 12; t.day = 20; /* not a planned day */
	assert(!sys_socPlanned(&t, &soc));
	t.day = 14; /* blank flash reads 655.35 %, rejected */
	assert(!sys_socPlanned(&t, &soc));

	printf("sys_socPlanned: all checks passed\n");
	return 0;
}
