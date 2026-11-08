/* AD5721, SPI. Speed display output. */

#include "sys_info_dac.h"
#include "sys_gpio.h"
#include "sys_spi.h"
#include "sys_time.h"

#define INFO_BUS SYS_SPI_M1

/* 0 V to 10 V range (gain 4) on a 3.3 V reference; 10 V is the output ceiling. */
#define INFO_VREFIN_V 3.3f
#define INFO_GAIN 4.0f
#define INFO_STEPS 4096.0f
#define INFO_MAX_V 10.0f
#define INFO_MAX_CODE ((uint16_t)(INFO_MAX_V / (INFO_GAIN * INFO_VREFIN_V) * INFO_STEPS))

#define CMD_NOP          0x00U
#define CMD_WRITE_UPDATE 0x03U
#define CMD_WRITE_CTRL   0x04U
#define CMD_READ_CTRL    0x0CU

/* Control register: range 0 V to 10 V, zero-scale power-up and clear. */
#define CTRL_VALUE 0x0001U
#define CTRL_MASK  0x07DFU

#define RESET_US 1000U

static uint8_t transfer(uint8_t cmd, uint16_t data, uint8_t *rx) {
	uint8_t tx[3] = { cmd, (uint8_t)(data >> 8), (uint8_t)data };
	uint8_t ok;

	sys_gpioSet(SPI2_SCSN, LO);
	ok = sys_spiTransfer(INFO_BUS, tx, rx, sizeof(tx));
	sys_gpioSet(SPI2_SCSN, HI);
	return ok;
}

uint8_t sys_infoDacInit(void) {
	uint8_t rx[3];

	sys_gpioSet(SPI2_SCSN, HI);
	sys_gpioSet(RESET_INFO_DAC, LO);
	sys_delayUs(RESET_US);
	sys_gpioSet(RESET_INFO_DAC, HI);
	sys_delayUs(RESET_US);

	if (!transfer(CMD_WRITE_CTRL, CTRL_VALUE, rx) ||
	    !transfer(CMD_WRITE_UPDATE, 0U, rx) ||
	    !transfer(CMD_READ_CTRL, 0U, rx) ||
	    !transfer(CMD_NOP, 0U, rx)) {
		return FALSE;
	}
	return ((uint16_t)((rx[1] << 8) | rx[2]) & CTRL_MASK) == CTRL_VALUE;
}

uint8_t sys_infoDacSetCode(uint16_t code) {
	uint8_t rx[3];

	if (code > INFO_MAX_CODE) {
		code = INFO_MAX_CODE;
	}
	return transfer(CMD_WRITE_UPDATE, (uint16_t)(code << 4), rx);
}

uint8_t sys_infoDacSetVolts(float volts) {
	float code = volts / (INFO_GAIN * INFO_VREFIN_V) * INFO_STEPS + 0.5f;

	if (!(code > 0.0f)) {
		code = 0.0f;
	}
	if (code > (float)INFO_MAX_CODE) {
		code = (float)INFO_MAX_CODE;
	}
	return sys_infoDacSetCode((uint16_t)code);
}
