/* USART1 debug. */

#include "sys_uart.h"
#include "main.h"

#define RX_SIZE 256U
#define TX_TIMEOUT_MS 100U

extern UART_HandleTypeDef huart1;

static uint8_t rxBuf[RX_SIZE];
static volatile uint16_t head = 0;
static uint16_t tail = 0;

static uint8_t startRx(void) {
	return HAL_UARTEx_ReceiveToIdle_DMA(&huart1, rxBuf, RX_SIZE) == HAL_OK;
}

uint8_t sys_uartInit(void) {
	head = 0;
	tail = 0;
	return startRx();
}

uint16_t sys_uartRead(uint8_t *dst, uint16_t max) {
	uint16_t h = head;
	uint16_t n = 0;

	__DMB();
	while (tail != h && n < max) {
		dst[n++] = rxBuf[tail];
		tail = (uint16_t)((tail + 1U) % RX_SIZE);
	}
	return n;
}

uint8_t sys_uartWrite(const uint8_t *data, uint16_t len) {
	return HAL_UART_Transmit(&huart1, (uint8_t *)data, len, TX_TIMEOUT_MS) == HAL_OK;
}

void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t size) {
	if (huart == &huart1) {
		head = (uint16_t)(size % RX_SIZE);
	}
}

/* On error, restart reception and drop unread bytes. */
void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart) {
	if (huart == &huart1) {
		HAL_UART_AbortReceive(huart);
		head = 0;
		tail = 0;
		startRx();
	}
}
