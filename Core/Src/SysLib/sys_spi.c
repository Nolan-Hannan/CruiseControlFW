/* SPI1/SPI2 bus transfers. */

#include "sys_spi.h"
#include "main.h"
#include <stddef.h>
#include <string.h>

#define SPI_TIMEOUT_MS 10U

extern SPI_HandleTypeDef hspi1;
extern SPI_HandleTypeDef hspi2;

static SPI_HandleTypeDef *getHandle(sys_spiBus_t bus) {
	switch (bus) {
	case SYS_SPI_M0: return &hspi1;
	case SYS_SPI_M1: return &hspi2;
	default:       return NULL;
	}
}

uint8_t sys_spiWrite(sys_spiBus_t bus, const uint8_t *tx, uint16_t len) {
	SPI_HandleTypeDef *handle = getHandle(bus);
	if (handle == NULL || tx == NULL || len == 0U) {
		return FALSE;
	}
	return HAL_SPI_Transmit(handle, (uint8_t *)tx, len, SPI_TIMEOUT_MS) == HAL_OK;
}

uint8_t sys_spiRead(sys_spiBus_t bus, uint8_t *rx, uint16_t len) {
	SPI_HandleTypeDef *handle = getHandle(bus);
	if (handle == NULL || rx == NULL || len == 0U) {
		return FALSE;
	}
	memset(rx, 0, len);
	return HAL_SPI_Receive(handle, rx, len, SPI_TIMEOUT_MS) == HAL_OK;
}

uint8_t sys_spiTransfer(sys_spiBus_t bus, const uint8_t *tx, uint8_t *rx, uint16_t len) {
	SPI_HandleTypeDef *handle = getHandle(bus);
	if (handle == NULL || tx == NULL || rx == NULL || len == 0U) {
		return FALSE;
	}
	return HAL_SPI_TransmitReceive(handle, (uint8_t *)tx, rx, len, SPI_TIMEOUT_MS) == HAL_OK;
}
