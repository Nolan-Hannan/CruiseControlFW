/* SOC curve storage in the A/B banks of the M95P32 flash. */

#ifndef SYS_SOC_H
#define SYS_SOC_H

#include "defines.h"
#include "eep_addr.h"
#include "sys_time.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Result codes, 0 is success. Codes 1 to 3 belong to the CLI. */
#define SYS_SOC_OK         0U
#define SYS_SOC_ERR_HEADER 4U /* bad header, stop symbol or length */
#define SYS_SOC_ERR_CRC    5U
#define SYS_SOC_ERR_FLASH  6U
#define SYS_SOC_ERR_VERIFY 7U /* read-back differs from what was received */
#define SYS_SOC_ERR_NONE   8U /* no curve stored */

typedef enum {
	SYS_SOC_BANK_A,
	SYS_SOC_BANK_B
} sys_socBank_t;

typedef struct {
	uint8_t month;
	uint8_t day;
	uint16_t year;
	uint8_t startHour;
	uint8_t startMin;
	uint8_t stopHour;
	uint8_t stopMin;
} sys_socDay_t;

typedef struct {
	sys_socBank_t bank;
	uint16_t record;
	uint8_t version;
	uint8_t days;
	sys_socDay_t day[EEP_SOC_DAYS];
} sys_socInfo_t;

/* Download: Begin, then Block for each received block, then Finish to commit. */
uint8_t sys_socBegin(void);
uint8_t sys_socBlock(const uint8_t *data, uint16_t len);
uint8_t sys_socFinish(void);
void sys_socAbort(void);

/* Rechecks the active bank in flash. */
uint8_t sys_socVerify(void);
uint8_t sys_socGetInfo(sys_socInfo_t *info);

/*
 * Planned SOC in percent at time t, interpolated between the 10 s samples. FALSE if there
 * is no curve, t is outside every planned day, or the stored sample is not 0 to 100 %.
 * The day headers are taken to be in the same timebase as t (UTC from the wall clock).
 */
uint8_t sys_socPlanned(const world_time_t *t, float *soc_percent);

/* Code of the last rejected block, and a short reason for the last failure. */
uint8_t sys_socLastError(void);
const char *sys_socDetail(void);

#ifdef __cplusplus
}
#endif

#endif /* SYS_SOC_H */
