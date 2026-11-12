/* SOC curve in two flash banks. An append-only pointer log names the active bank. */

#include "sys_soc.h"
#include "sys_crc.h"
#include "sys_flash.h"
#include <stddef.h>
#include <string.h>

#define PAGE_SIZE    512U
#define LOG_CHUNK    128U
#define VERIFY_CHUNK 256U

#define SOC_VERSION     0x01U
#define SOC_SAMPLES     2880U
#define SOC_INTERVAL_S  10U
#define SOC_STOP_SYMBOL 0xABCDU

/* CRC regions: header, each day (data and stop character), then the whole file. */
#define REGION_COUNT (EEP_SOC_DAYS + 2U)

typedef struct {
	uint8_t valid;
	sys_socBank_t bank;
	uint16_t slot;
	uint16_t nextFree;
} logState_t;

typedef struct {
	uint32_t crc[REGION_COUNT];
	uint8_t stored[REGION_COUNT][4];
	uint8_t stop[EEP_SOC_DAYS][2];
	uint8_t headerOk;
} scan_t;

static const char *const crcNames[REGION_COUNT] = {
	"header crc", "day 1 crc", "day 2 crc", "day 3 crc", "end crc"
};
static const char *const stopNames[EEP_SOC_DAYS] = {
	"day 1 stop", "day 2 stop", "day 3 stop"
};

static struct {
	uint8_t active;
	uint8_t error;
	sys_socBank_t bank;
	uint32_t base;
	uint32_t received;
	uint16_t pageFill;
	uint8_t page[PAGE_SIZE];
	scan_t scan;
} ctx;

static const char *detail = "";

/* Header of the active curve, read once and reused by sys_socPlanned until the next download. */
static sys_socInfo_t planInfo;
static uint8_t planLoaded = FALSE;

static uint8_t fail(uint8_t code, const char *why) {
	detail = why;
	return code;
}

static uint16_t le16(const uint8_t *p) {
	return (uint16_t)(p[0] | (p[1] << 8));
}

static uint32_t le32(const uint8_t *p) {
	return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static void putLe32(uint8_t *p, uint32_t value) {
	p[0] = (uint8_t)value;
	p[1] = (uint8_t)(value >> 8);
	p[2] = (uint8_t)(value >> 16);
	p[3] = (uint8_t)(value >> 24);
}

static uint32_t bankBase(sys_socBank_t bank) {
	return (bank == SYS_SOC_BANK_A) ? EEP_SOC_BANK_A_ADDR : EEP_SOC_BANK_B_ADDR;
}

static uint8_t isBlank(const uint8_t *p, uint8_t len) {
	while (len > 0U) {
		if (*p != 0xFFU) {
			return FALSE;
		}
		p++;
		len--;
	}
	return TRUE;
}

/* ---- Pointer log ---- */

static uint8_t recordValid(const uint8_t *rec, sys_socBank_t *bank) {
	if (memcmp(&rec[EEP_PTR_ID_OFF], "BANK", 4U) != 0 ||
	    le32(&rec[EEP_PTR_CRC_OFF]) != sys_crc32(0U, rec, EEP_PTR_CRC_OFF)) {
		return FALSE;
	}
	if (rec[EEP_PTR_BANK_OFF] == EEP_PTR_BANK_A && rec[EEP_PTR_INV_OFF] == EEP_PTR_INV_A) {
		*bank = SYS_SOC_BANK_A;
		return TRUE;
	}
	if (rec[EEP_PTR_BANK_OFF] == EEP_PTR_BANK_B && rec[EEP_PTR_INV_OFF] == EEP_PTR_INV_B) {
		*bank = SYS_SOC_BANK_B;
		return TRUE;
	}
	return FALSE;
}

/* The last valid record names the active bank; the first blank slot ends the log. */
static uint8_t scanLog(logState_t *log) {
	uint8_t buf[LOG_CHUNK];
	uint32_t offset;
	uint8_t i;

	log->valid = FALSE;
	log->nextFree = EEP_PTR_REC_COUNT;
	for (offset = 0U; offset < EEP_PTR_LOG_SIZE; offset += LOG_CHUNK) {
		if (!sys_flashRead(EEP_PTR_LOG_ADDR + offset, buf, LOG_CHUNK)) {
			return FALSE;
		}
		for (i = 0U; i < LOG_CHUNK / EEP_PTR_REC_SIZE; i++) {
			const uint8_t *rec = &buf[i * EEP_PTR_REC_SIZE];
			uint16_t slot = (uint16_t)((offset / EEP_PTR_REC_SIZE) + i);
			sys_socBank_t bank;

			if (isBlank(rec, EEP_PTR_REC_SIZE)) {
				log->nextFree = slot;
				return TRUE;
			}
			if (recordValid(rec, &bank)) {
				log->valid = TRUE;
				log->bank = bank;
				log->slot = slot;
			}
		}
	}
	return TRUE;
}

static uint8_t appendRecord(sys_socBank_t bank, const logState_t *log) {
	uint8_t rec[EEP_PTR_REC_SIZE];
	uint8_t check[EEP_PTR_REC_SIZE];
	uint16_t slot = log->nextFree;
	uint32_t addr;

	memset(rec, 0xFF, sizeof(rec));
	memcpy(&rec[EEP_PTR_ID_OFF], "BANK", 4U);
	rec[EEP_PTR_BANK_OFF] = (bank == SYS_SOC_BANK_A) ? EEP_PTR_BANK_A : EEP_PTR_BANK_B;
	rec[EEP_PTR_INV_OFF] = (bank == SYS_SOC_BANK_A) ? EEP_PTR_INV_A : EEP_PTR_INV_B;
	putLe32(&rec[EEP_PTR_CRC_OFF], sys_crc32(0U, rec, EEP_PTR_CRC_OFF));

	if (slot >= EEP_PTR_REC_COUNT) {
		if (!sys_flashEraseSectorBlock(EEP_PTR_LOG_ADDR)) {
			return FALSE;
		}
		slot = 0U;
	}
	addr = EEP_PTR_LOG_ADDR + (uint32_t)slot * EEP_PTR_REC_SIZE;
	return sys_flashProgramBlock(addr, rec, EEP_PTR_REC_SIZE) &&
	       sys_flashRead(addr, check, EEP_PTR_REC_SIZE) &&
	       memcmp(rec, check, EEP_PTR_REC_SIZE) == 0;
}

/* ---- File checks ---- */

static void regionSpan(uint8_t region, uint32_t *start, uint32_t *length) {
	if (region == 0U) {
		*start = 0U;
		*length = EEP_SOC_HDR_CRC_OFF;
	} else if (region <= EEP_SOC_DAYS) {
		*start = EEP_SOC_DATA_OFF + (uint32_t)(region - 1U) * EEP_SOC_DAY_STRIDE;
		*length = EEP_SOC_DATA_SIZE + 2U;
	} else {
		*start = 0U;
		*length = EEP_SOC_END_CRC_OFF;
	}
}

/* Overlap of the chunk [off, off + len) with [start, start + length). Returns its size. */
static uint32_t overlap(uint32_t off, uint32_t len, uint32_t start, uint32_t length, uint32_t *from) {
	uint32_t lo = (start > off) ? start : off;
	uint32_t hi = (start + length < off + len) ? (start + length) : (off + len);

	if (hi <= lo) {
		return 0U;
	}
	*from = lo;
	return hi - lo;
}

static uint8_t headerFieldsOk(const uint8_t *h) {
	return memcmp(&h[EEP_SOC_ID_OFF], "SOC1", 4U) == 0 &&
	       h[EEP_SOC_VERSION_OFF] == SOC_VERSION &&
	       h[EEP_SOC_DAYS_OFF] == EEP_SOC_DAYS &&
	       le16(&h[EEP_SOC_SAMPLES_OFF]) == SOC_SAMPLES &&
	       le16(&h[EEP_SOC_INTERVAL_OFF]) == SOC_INTERVAL_S &&
	       le16(&h[EEP_SOC_STOP_OFF]) == SOC_STOP_SYMBOL;
}

static void scanReset(scan_t *s) {
	memset(s, 0, sizeof(*s));
}

/* Feed the file in order, in chunks of any size; the first chunk must hold the whole header. */
static void scanFeed(scan_t *s, uint32_t off, const uint8_t *buf, uint16_t len) {
	uint8_t region;
	uint8_t day;
	uint32_t from;
	uint32_t n;

	for (region = 0U; region < REGION_COUNT; region++) {
		uint32_t start;
		uint32_t length;

		regionSpan(region, &start, &length);
		n = overlap(off, len, start, length, &from);
		if (n > 0U) {
			s->crc[region] = sys_crc32(s->crc[region], &buf[from - off], n);
		}
		n = overlap(off, len, start + length, 4U, &from);
		if (n > 0U) {
			memcpy(&s->stored[region][from - (start + length)], &buf[from - off], n);
		}
	}
	for (day = 0U; day < EEP_SOC_DAYS; day++) {
		uint32_t at = EEP_SOC_DATA_OFF + (uint32_t)day * EEP_SOC_DAY_STRIDE + EEP_SOC_DATA_SIZE;

		n = overlap(off, len, at, 2U, &from);
		if (n > 0U) {
			memcpy(&s->stop[day][from - at], &buf[from - off], n);
		}
	}
	if (off == 0U) {
		s->headerOk = (len >= EEP_SOC_DATA_OFF && headerFieldsOk(buf)) ? TRUE : FALSE;
	}
}

static uint8_t headerVerdict(const scan_t *s) {
	if (!s->headerOk) {
		return fail(SYS_SOC_ERR_HEADER, "header");
	}
	if (s->crc[0] != le32(s->stored[0])) {
		return fail(SYS_SOC_ERR_CRC, crcNames[0]);
	}
	return SYS_SOC_OK;
}

static uint8_t scanVerdict(const scan_t *s) {
	uint8_t day;
	uint8_t code = headerVerdict(s);

	if (code != SYS_SOC_OK) {
		return code;
	}
	for (day = 0U; day < EEP_SOC_DAYS; day++) {
		if (le16(s->stop[day]) != SOC_STOP_SYMBOL) {
			return fail(SYS_SOC_ERR_HEADER, stopNames[day]);
		}
		if (s->crc[day + 1U] != le32(s->stored[day + 1U])) {
			return fail(SYS_SOC_ERR_CRC, crcNames[day + 1U]);
		}
	}
	if (s->crc[REGION_COUNT - 1U] != le32(s->stored[REGION_COUNT - 1U])) {
		return fail(SYS_SOC_ERR_CRC, crcNames[REGION_COUNT - 1U]);
	}
	return SYS_SOC_OK;
}

static uint8_t readScan(uint32_t base, scan_t *s) {
	uint8_t buf[VERIFY_CHUNK];
	uint32_t off;

	scanReset(s);
	for (off = 0U; off < EEP_SOC_FILE_SIZE; off += VERIFY_CHUNK) {
		if (!sys_flashRead(base + off, buf, VERIFY_CHUNK)) {
			return FALSE;
		}
		scanFeed(s, off, buf, VERIFY_CHUNK);
	}
	return TRUE;
}

/* ---- Download ---- */

uint8_t sys_socBegin(void) {
	logState_t log;

	ctx.active = FALSE;
	detail = "";
	if (!scanLog(&log)) {
		return fail(SYS_SOC_ERR_FLASH, "flash read");
	}
	ctx.bank = (log.valid && log.bank == SYS_SOC_BANK_A) ? SYS_SOC_BANK_B : SYS_SOC_BANK_A;
	ctx.base = bankBase(ctx.bank);
	ctx.received = 0U;
	ctx.pageFill = 0U;
	ctx.error = SYS_SOC_OK;
	scanReset(&ctx.scan);
	ctx.active = TRUE;
	return SYS_SOC_OK;
}

uint8_t sys_socBlock(const uint8_t *data, uint16_t len) {
	uint32_t offset = ctx.received;

	if (!ctx.active || data == NULL) {
		ctx.error = fail(SYS_SOC_ERR_HEADER, "not started");
		return FALSE;
	}
	if (len > EEP_SOC_FILE_SIZE - offset) {
		ctx.error = fail(SYS_SOC_ERR_HEADER, "too long");
		return FALSE;
	}
	scanFeed(&ctx.scan, offset, data, len);
	if (offset == 0U) {
		uint8_t code = headerVerdict(&ctx.scan);

		if (code != SYS_SOC_OK) {
			ctx.error = code;
			return FALSE;
		}
	}
	while (len > 0U) {
		uint16_t n = (uint16_t)(PAGE_SIZE - ctx.pageFill);

		if (n > len) {
			n = len;
		}
		memcpy(&ctx.page[ctx.pageFill], data, n);
		ctx.pageFill = (uint16_t)(ctx.pageFill + n);
		ctx.received += n;
		data += n;
		len = (uint16_t)(len - n);
		if (ctx.pageFill == PAGE_SIZE) {
			if (!sys_flashWriteBlock(ctx.base + ctx.received - PAGE_SIZE, ctx.page, PAGE_SIZE)) {
				ctx.error = fail(SYS_SOC_ERR_FLASH, "flash write");
				return FALSE;
			}
			ctx.pageFill = 0U;
		}
	}
	return TRUE;
}

uint8_t sys_socFinish(void) {
	scan_t check;
	logState_t log;
	uint8_t code;

	if (!ctx.active) {
		return fail(SYS_SOC_ERR_HEADER, "not started");
	}
	ctx.active = FALSE;
	if (ctx.received != EEP_SOC_FILE_SIZE) {
		return fail(SYS_SOC_ERR_HEADER, "length");
	}
	code = scanVerdict(&ctx.scan);
	if (code != SYS_SOC_OK) {
		return code;
	}
	if (!readScan(ctx.base, &check)) {
		return fail(SYS_SOC_ERR_FLASH, "flash read");
	}
	if (scanVerdict(&check) != SYS_SOC_OK) {
		return fail(SYS_SOC_ERR_VERIFY, "readback");
	}
	if (!scanLog(&log) || !appendRecord(ctx.bank, &log)) {
		return fail(SYS_SOC_ERR_FLASH, "pointer");
	}
	planLoaded = FALSE;
	return SYS_SOC_OK;
}

void sys_socAbort(void) {
	ctx.active = FALSE;
}

/* ---- Active curve ---- */

uint8_t sys_socVerify(void) {
	logState_t log;
	scan_t check;

	detail = "";
	if (!scanLog(&log)) {
		return fail(SYS_SOC_ERR_FLASH, "flash read");
	}
	if (!log.valid) {
		return fail(SYS_SOC_ERR_NONE, "no curve");
	}
	if (!readScan(bankBase(log.bank), &check)) {
		return fail(SYS_SOC_ERR_FLASH, "flash read");
	}
	return scanVerdict(&check);
}

uint8_t sys_socGetInfo(sys_socInfo_t *info) {
	logState_t log;
	uint8_t hdr[EEP_SOC_DATA_OFF];
	uint8_t i;

	detail = "";
	if (info == NULL || !scanLog(&log)) {
		return fail(SYS_SOC_ERR_FLASH, "flash read");
	}
	if (!log.valid) {
		return fail(SYS_SOC_ERR_NONE, "no curve");
	}
	if (!sys_flashRead(bankBase(log.bank), hdr, sizeof(hdr))) {
		return fail(SYS_SOC_ERR_FLASH, "flash read");
	}
	info->bank = log.bank;
	info->record = log.slot;
	info->version = hdr[EEP_SOC_VERSION_OFF];
	info->days = (hdr[EEP_SOC_DAYS_OFF] > EEP_SOC_DAYS) ? (uint8_t)EEP_SOC_DAYS : hdr[EEP_SOC_DAYS_OFF];
	for (i = 0U; i < info->days; i++) {
		const uint8_t *d = &hdr[EEP_SOC_DAYHDR_OFF + (uint32_t)i * EEP_SOC_DAYHDR_SIZE];

		info->day[i].month = d[EEP_SOC_DAY_MONTH];
		info->day[i].day = d[EEP_SOC_DAY_DAY];
		info->day[i].year = le16(&d[EEP_SOC_DAY_YEAR]);
		info->day[i].startHour = d[EEP_SOC_DAY_START_H];
		info->day[i].startMin = d[EEP_SOC_DAY_START_M];
		info->day[i].stopHour = d[EEP_SOC_DAY_STOP_H];
		info->day[i].stopMin = d[EEP_SOC_DAY_STOP_M];
	}
	return SYS_SOC_OK;
}

uint8_t sys_socPlanned(const world_time_t *t, float *soc_percent) {
	uint8_t i;

	if (t == NULL || soc_percent == NULL) {
		return FALSE;
	}
	if (!planLoaded) {
		if (sys_socGetInfo(&planInfo) != SYS_SOC_OK) {
			return FALSE;
		}
		planLoaded = TRUE;
	}
	for (i = 0U; i < planInfo.days; i++) {
		const sys_socDay_t *d = &planInfo.day[i];
		int32_t sec = ((int32_t)t->hours - d->startHour) * 3600 + ((int32_t)t->min - d->startMin) * 60 + t->sec;
		int32_t len = ((int32_t)d->stopHour - d->startHour) * 3600 + ((int32_t)d->stopMin - d->startMin) * 60;
		uint8_t raw[4];
		uint32_t index;
		uint8_t count;
		float a;
		float b;

		if (t->year != d->year || t->month != d->month || t->day != d->day) {
			continue;
		}
		if (sec < 0 || sec > len || sec >= (int32_t)(SOC_SAMPLES * SOC_INTERVAL_S)) {
			return FALSE;
		}
		index = (uint32_t)sec / SOC_INTERVAL_S;
		count = (index + 1U < SOC_SAMPLES) ? 2U : 1U; /* the last sample has no successor */
		if (!sys_flashRead(bankBase(planInfo.bank) + EEP_SOC_DATA_OFF + (uint32_t)i * EEP_SOC_DAY_STRIDE +
		                   index * 2U, raw, (uint16_t)(count * 2U))) {
			return FALSE;
		}
		/* Samples are percent times 100. */
		a = (float)le16(&raw[0]);
		b = (count == 2U) ? (float)le16(&raw[2]) : a;
		if (a > 10000.0f || b > 10000.0f) {
			return FALSE;
		}
		*soc_percent = (a + (b - a) * (float)((uint32_t)sec % SOC_INTERVAL_S) / (float)SOC_INTERVAL_S) / 100.0f;
		return TRUE;
	}
	return FALSE;
}

uint8_t sys_socLastError(void) {
	return ctx.error;
}

const char *sys_socDetail(void) {
	return detail;
}
