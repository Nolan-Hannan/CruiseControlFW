/* XMODEM-CRC receiver on the debug UART. 128-byte blocks only. */

#include "sys_xmodem.h"
#include "sys_crc.h"
#include "sys_time.h"
#include "sys_uart.h"
#include <stddef.h>

#define SOH 0x01U
#define STX 0x02U
#define EOT 0x04U
#define ACK 0x06U
#define NAK 0x15U
#define CAN 0x18U
#define CRC_REQUEST 0x43U

#define BLOCK_SIZE 128U
#define FRAME_SIZE 133U /* SOH, number, complement, data, CRC16 high and low */

#define POLL_MS            3000U
#define HANDSHAKE_TOTAL_MS 60000U
#define SILENCE_MS         1000U
#define MAX_FAILURES       10U

#define RX_SLICE      32U
#define RX_SLICES_MAX 16U

typedef enum {
	STATE_IDLE,
	STATE_FRAME,
	STATE_PURGE,
	STATE_FINISHED
} state_t;

static sys_timer_t pollTimer = { .timerLengthMs = POLL_MS };
static sys_timer_t totalTimer = { .timerLengthMs = HANDSHAKE_TOTAL_MS };
static sys_timer_t silenceTimer = { .timerLengthMs = SILENCE_MS };

static sys_xmodemBlockFn blockFn = NULL;
static sys_xmodemStatus_t result = SYS_XMODEM_FAILED;
static state_t state = STATE_FINISHED;
static uint8_t frame[FRAME_SIZE];
static uint8_t fill = 0U;
static uint8_t expected = 1U;
static uint8_t started = FALSE;
static uint8_t failures = 0U;
static uint8_t cancelCount = 0U;

static void sendByte(uint8_t byte) {
	(void)sys_uartWrite(&byte, 1U);
}

static void drain(void) {
	uint8_t junk[RX_SLICE];
	uint8_t slice;

	for (slice = 0U; slice < RX_SLICES_MAX; slice++) {
		if (sys_uartRead(junk, RX_SLICE) < RX_SLICE) {
			break;
		}
	}
}

static void finish(sys_xmodemStatus_t status) {
	result = status;
	state = STATE_FINISHED;
	drain();
}

static void cancelSender(sys_xmodemStatus_t status) {
	sendByte(CAN);
	sendByte(CAN);
	finish(status);
}

static void toIdle(void) {
	state = STATE_IDLE;
	sys_timerInit(&pollTimer);
}

/* Counts a failure. Returns TRUE if the transfer was ended. */
static uint8_t tooManyFailures(void) {
	failures++;
	if (failures >= MAX_FAILURES) {
		cancelSender(SYS_XMODEM_TIMEOUT);
		return TRUE;
	}
	return FALSE;
}

static void reject(void) {
	if (!tooManyFailures()) {
		sendByte(NAK);
		toIdle();
	}
}

static void checkFrame(void) {
	const uint8_t *data = &frame[3];
	uint16_t crcRx = (uint16_t)(((uint16_t)frame[131] << 8) | frame[132]);

	if ((uint8_t)(frame[1] ^ frame[2]) != 0xFFU || sys_crc16Xmodem(0U, data, BLOCK_SIZE) != crcRx) {
		reject();
	} else if (started && frame[1] == (uint8_t)(expected - 1U)) {
		/* The sender repeated a block because our ACK was lost. */
		if (!tooManyFailures()) {
			sendByte(ACK);
			toIdle();
		}
	} else if (frame[1] != expected) {
		reject();
	} else if (!blockFn(data, BLOCK_SIZE)) {
		cancelSender(SYS_XMODEM_FAILED);
	} else {
		sendByte(ACK);
		expected++;
		started = TRUE;
		failures = 0U;
		toIdle();
	}
}

static void handleByte(uint8_t byte) {
	switch (state) {
	case STATE_IDLE:
		if (byte == CAN) {
			cancelCount++;
			if (cancelCount >= 2U) {
				finish(SYS_XMODEM_CANCELLED);
			}
			return;
		}
		cancelCount = 0U;
		if (byte == SOH) {
			frame[0] = byte;
			fill = 1U;
			state = STATE_FRAME;
			sys_timerInit(&silenceTimer);
		} else if (byte == STX) {
			state = STATE_PURGE;
			sys_timerInit(&silenceTimer);
		} else if (byte == EOT) {
			sendByte(ACK);
			finish(SYS_XMODEM_DONE);
		}
		break;
	case STATE_FRAME:
		frame[fill++] = byte;
		sys_timerInit(&silenceTimer);
		if (fill >= FRAME_SIZE) {
			checkFrame();
		}
		break;
	case STATE_PURGE:
		sys_timerInit(&silenceTimer);
		break;
	default:
		break;
	}
}

static void checkTimers(void) {
	switch (state) {
	case STATE_IDLE:
		if (!started) {
			if (sys_checkOutTimerMs(&totalTimer)) {
				cancelSender(SYS_XMODEM_TIMEOUT);
			} else if (sys_checkOutTimerMs(&pollTimer)) {
				sendByte(CRC_REQUEST);
			}
		} else if (sys_checkOutTimerMs(&pollTimer)) {
			reject();
		}
		break;
	case STATE_FRAME:
	case STATE_PURGE:
		if (sys_checkOutTimerMs(&silenceTimer)) {
			reject();
		}
		break;
	default:
		break;
	}
}

void sys_xmodemStart(sys_xmodemBlockFn onBlock) {
	blockFn = onBlock;
	if (onBlock == NULL) {
		finish(SYS_XMODEM_FAILED);
		return;
	}
	drain();
	fill = 0U;
	expected = 1U;
	started = FALSE;
	failures = 0U;
	cancelCount = 0U;
	result = SYS_XMODEM_RUNNING;
	state = STATE_IDLE;
	sys_timerInit(&pollTimer);
	sys_timerInit(&totalTimer);
}

sys_xmodemStatus_t sys_xmodemStep(void) {
	uint8_t chunk[RX_SLICE];
	uint8_t slice;
	uint16_t count;
	uint16_t i;

	for (slice = 0U; slice < RX_SLICES_MAX && result == SYS_XMODEM_RUNNING; slice++) {
		count = sys_uartRead(chunk, RX_SLICE);
		for (i = 0U; i < count && result == SYS_XMODEM_RUNNING; i++) {
			handleByte(chunk[i]);
		}
		if (count < RX_SLICE) {
			break;
		}
	}
	if (result == SYS_XMODEM_RUNNING) {
		checkTimers();
	}
	return result;
}

void sys_xmodemAbort(void) {
	if (state != STATE_FINISHED) {
		cancelSender(SYS_XMODEM_CANCELLED);
	}
}
