/* BMI323, SPI. Gradient and acceleration. */

#include "sys_imu.h"
#include "sys_gpio.h"
#include "sys_spi.h"
#include "sys_time.h"
#include <stddef.h>

#define IMU_BUS SYS_SPI_M0

#define REG_CHIP_ID     0x00U
#define REG_ERR         0x01U
#define REG_DATA        0x03U
#define REG_ACC_CONF    0x20U
#define REG_GYR_CONF    0x21U
#define REG_IO_INT_CTRL 0x38U
#define REG_INT_MAP1    0x3AU
#define REG_INT_MAP2    0x3BU
#define REG_CMD         0x7EU

#define READ_FLAG      0x80U
#define CHIP_ID_VALUE  0x43U
#define ERR_FATAL      0x0001U
#define CMD_SOFT_RESET 0xDEAFU
#define SAMPLE_INVALID 0x8000U

/* High-performance mode, 100 Hz, no averaging; accelerometer +-4 g, gyroscope +-500 deg/s. */
#define ACC_CONF_VALUE 0x7018U
#define GYR_CONF_VALUE 0x7028U

#define ACC_LSB_PER_G   8192.0f
#define GYR_LSB_PER_DPS 65.536f
#define TEMP_LSB_PER_C  512.0f
#define TEMP_OFFSET_C   23.0f

/* Accelerometer x/y/z, gyroscope x/y/z, temperature. */
#define DATA_WORDS 7U
#define MAX_WORDS  DATA_WORDS
#define AXIS_WORDS 6U

#define POWER_ON_US      2000U
#define SOFT_RESET_US    5000U
#define SUSPEND_IDLE_US  500U

static uint8_t readRegs(uint8_t reg, uint16_t *dst, uint8_t words) {
	uint8_t tx[2U + 2U * MAX_WORDS] = { 0U };
	uint8_t rx[2U + 2U * MAX_WORDS];
	uint8_t ok;
	uint8_t i;

	if (words == 0U || words > MAX_WORDS) {
		return FALSE;
	}
	tx[0] = reg | READ_FLAG;
	sys_gpioSet(SPI1_SCSN_IMU, LO);
	ok = sys_spiTransfer(IMU_BUS, tx, rx, (uint16_t)(2U + 2U * words));
	sys_gpioSet(SPI1_SCSN_IMU, HI);
	if (ok) {
		/* rx[0] is clocked during the address, rx[1] is the dummy byte. */
		for (i = 0U; i < words; i++) {
			dst[i] = (uint16_t)(rx[2U + 2U * i] | (rx[3U + 2U * i] << 8));
		}
	}
	return ok;
}

static uint8_t writeReg(uint8_t reg, uint16_t value) {
	uint8_t tx[3] = { reg, (uint8_t)value, (uint8_t)(value >> 8) };
	uint8_t ok;

	sys_gpioSet(SPI1_SCSN_IMU, LO);
	ok = sys_spiWrite(IMU_BUS, tx, sizeof(tx));
	sys_gpioSet(SPI1_SCSN_IMU, HI);
	return ok;
}

/* The device needs a long interface idle time between accesses until it leaves suspend. */
static uint8_t initWrite(uint8_t reg, uint16_t value) {
	uint8_t ok = writeReg(reg, value);

	sys_delayUs(SUSPEND_IDLE_US);
	return ok;
}

static uint8_t initRead(uint8_t reg, uint16_t *value) {
	uint8_t ok = readRegs(reg, value, 1U);

	sys_delayUs(SUSPEND_IDLE_US);
	return ok;
}

uint8_t sys_imuInit(void) {
	uint16_t regValue;

	sys_gpioSet(SPI1_SCSN_IMU, HI);
	sys_delayUs(POWER_ON_US);

	/* The first read after power-up or reset switches the interface to SPI; its data is invalid. */
	(void)initRead(REG_CHIP_ID, &regValue);
	if (!initWrite(REG_CMD, CMD_SOFT_RESET)) {
		return FALSE;
	}
	sys_delayUs(SOFT_RESET_US);
	(void)initRead(REG_CHIP_ID, &regValue);

	if (!initRead(REG_CHIP_ID, &regValue) || (regValue & 0xFFU) != CHIP_ID_VALUE) {
		return FALSE;
	}
	if (!initRead(REG_ERR, &regValue) || (regValue & ERR_FATAL) != 0U) {
		return FALSE;
	}

	/* INT1 and INT2 are not connected to the MCU: outputs off, nothing mapped. */
	if (!initWrite(REG_IO_INT_CTRL, 0U) || !initWrite(REG_INT_MAP1, 0U) ||
	    !initWrite(REG_INT_MAP2, 0U)) {
		return FALSE;
	}
	if (!initRead(REG_IO_INT_CTRL, &regValue) || regValue != 0U) {
		return FALSE;
	}

	if (!initWrite(REG_ACC_CONF, ACC_CONF_VALUE) || !initWrite(REG_GYR_CONF, GYR_CONF_VALUE)) {
		return FALSE;
	}
	return initRead(REG_ACC_CONF, &regValue) && regValue == ACC_CONF_VALUE &&
	       initRead(REG_GYR_CONF, &regValue) && regValue == GYR_CONF_VALUE;
}

uint8_t sys_imuReadRaw(sys_imuRaw_t *raw) {
	uint16_t rawWords[DATA_WORDS];
	uint8_t i;

	if (raw == NULL || !readRegs(REG_DATA, rawWords, (uint8_t)DATA_WORDS)) {
		return FALSE;
	}
	for (i = 0U; i < AXIS_WORDS; i++) {
		if (rawWords[i] == SAMPLE_INVALID) {
			return FALSE;
		}
	}
	raw->accX = (int16_t)rawWords[0];
	raw->accY = (int16_t)rawWords[1];
	raw->accZ = (int16_t)rawWords[2];
	raw->gyrX = (int16_t)rawWords[3];
	raw->gyrY = (int16_t)rawWords[4];
	raw->gyrZ = (int16_t)rawWords[5];
	raw->temp = (int16_t)rawWords[6];
	return TRUE;
}

uint8_t sys_imuRead(sys_imuData_t *data) {
	sys_imuRaw_t raw;

	if (data == NULL || !sys_imuReadRaw(&raw)) {
		return FALSE;
	}
	data->accX = (float)raw.accX / ACC_LSB_PER_G;
	data->accY = (float)raw.accY / ACC_LSB_PER_G;
	data->accZ = (float)raw.accZ / ACC_LSB_PER_G;
	data->gyrX = (float)raw.gyrX / GYR_LSB_PER_DPS;
	data->gyrY = (float)raw.gyrY / GYR_LSB_PER_DPS;
	data->gyrZ = (float)raw.gyrZ / GYR_LSB_PER_DPS;
	data->tempC = (float)raw.temp / TEMP_LSB_PER_C + TEMP_OFFSET_C;
	return TRUE;
}
