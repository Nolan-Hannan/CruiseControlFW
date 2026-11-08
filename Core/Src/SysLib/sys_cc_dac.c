/* MCP48CVD21, SPI. Throttle voltage output. */

#include "sys_cc_dac.h"
#include "sys_gpio.h"
#include "sys_spi.h"

#define DAC_BUS SYS_SPI_M0
#define DAC_VREF_V 3.3f
#define DAC_STEPS 4096.0f
#define DAC_MAX_CODE 4095U

#define ADDR_DAC0 0x00U
#define ADDR_VREF 0x08U
#define ADDR_PD   0x09U

#define CMD_WRITE 0x00U
#define CMD_READ  0x06U

/* A valid read returns seven high bits plus a high CMDERR bit. */
#define READ_VALID 0xFFU

static uint8_t transfer(uint8_t cmd, uint8_t addr, uint16_t data, uint8_t *rx) {
	uint8_t tx[3] = { (uint8_t)((addr << 3) | cmd), (uint8_t)(data >> 8), (uint8_t)data };
	uint8_t status;

	sys_gpioSet(SPI1_SCSN_DAC, LO);
	status = sys_spiTransfer(DAC_BUS, tx, rx, sizeof(tx));
	sys_gpioSet(SPI1_SCSN_DAC, HI);
	return status;
}

static uint8_t write(uint8_t addr, uint16_t data) {
	uint8_t rx[3];

	return transfer(CMD_WRITE, addr, data, rx);
}

static uint8_t verify(uint8_t addr, uint16_t expected) {
	uint8_t rx[3];

	if (!transfer(CMD_READ, addr, 0U, rx)) {
		return FALSE;
	}
	return rx[0] == READ_VALID && (uint16_t)((rx[1] << 8) | rx[2]) == expected;
}

uint8_t sys_ccDacInit(void) {
	sys_gpioSet(SPI1_SCSN_DAC, HI);
	return write(ADDR_DAC0, 0U) && write(ADDR_VREF, 0U) && write(ADDR_PD, 0U) &&
	       verify(ADDR_DAC0, 0U) && verify(ADDR_VREF, 0U);
}

uint8_t sys_ccDacSetCode(uint16_t code) {
	if (code > DAC_MAX_CODE) {
		code = DAC_MAX_CODE;
	}
	return write(ADDR_DAC0, code);
}

uint8_t sys_ccDacSetVolts(float volts) {
	float code = volts / DAC_VREF_V * DAC_STEPS + 0.5f;

	if (!(code > 0.0f)) {
		code = 0.0f;
	}
	if (code > (float)DAC_MAX_CODE) {
		code = (float)DAC_MAX_CODE;
	}
	return sys_ccDacSetCode((uint16_t)code);
}
