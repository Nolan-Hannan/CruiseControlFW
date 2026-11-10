/* UART command line. */

#include "cli.h"
#include "cc_state.h"
#include "sys_soc.h"
#include "sys_time.h"
#include "sys_uart.h"
#include "sys_xmodem.h"
#include <stddef.h>
#include <string.h>

#if CLI_ENABLED

#define LINE_MAX_LEN 64U
#define ARGV_MAX     8U
#define RX_CHUNK     16U
#define OUT_MAX      96U

#define SETTLE_MS 500U

#define BANNER "CruiseControlFW CLI, type help\r\n"
#define PROMPT "> "

typedef int (*cmdHandler_t)(int argc, char *argv[]);

typedef struct {
	const char *name;
	const char *help;
	cmdHandler_t handler;
} command_t;

static int cmdHelp(int argc, char *argv[]);
static int cmdPing(int argc, char *argv[]);
static int cmdDownload(int argc, char *argv[]);
static int cmdSoc(int argc, char *argv[]);

static const command_t commands[] = {
	{ "help",     "list commands",                    cmdHelp },
	{ "ping",     "reply pong",                       cmdPing },
	{ "download", "load the SOC curve by XMODEM-CRC", cmdDownload },
	{ "soc",      "soc info | soc verify",            cmdSoc },
};

#define COMMAND_COUNT (sizeof(commands) / sizeof(commands[0]))

static char line[LINE_MAX_LEN + 1U];
static uint8_t length = 0U;
static uint8_t overflow = FALSE;
static uint8_t previous = 0U;
static CLI_ModeStep modeStep = NULL;
static const char *errorNote = NULL;

static void append(char *buf, uint8_t *len, const char *str) {
	while (*str != '\0' && *len < OUT_MAX) {
		buf[(*len)++] = *str++;
	}
}

static void appendChar(char *buf, uint8_t *len, char c) {
	if (*len < OUT_MAX) {
		buf[(*len)++] = c;
	}
}

static void appendInt(char *buf, uint8_t *len, int value) {
	char digits[12];
	uint8_t count = 0U;
	unsigned int magnitude = (value < 0) ? (0U - (unsigned int)value) : (unsigned int)value;

	if (value < 0) {
		appendChar(buf, len, '-');
	}
	do {
		digits[count++] = (char)('0' + (magnitude % 10U));
		magnitude /= 10U;
	} while (magnitude > 0U);
	while (count > 0U) {
		appendChar(buf, len, digits[--count]);
	}
}

static void appendTwo(char *buf, uint8_t *len, int value) {
	if (value < 10) {
		appendChar(buf, len, '0');
	}
	appendInt(buf, len, value);
}

static void flush(const char *buf, uint8_t len) {
	if (len > 0U) {
		(void)sys_uartWrite((const uint8_t *)buf, len);
	}
}

static void reply(const char *text) {
	char out[OUT_MAX];
	uint8_t len = 0U;

	append(out, &len, text);
	append(out, &len, "\r\n");
	flush(out, len);
}

static void replyError(int code) {
	char out[OUT_MAX];
	uint8_t len = 0U;

	append(out, &len, "ERR ");
	appendInt(out, &len, code);
	if (errorNote != NULL && errorNote[0] != '\0') {
		appendChar(out, &len, ' ');
		append(out, &len, errorNote);
	}
	errorNote = NULL;
	append(out, &len, "\r\n");
	flush(out, len);
}

/* Passes a sys_soc code on, with its reason shown after the ERR code. */
static int socResult(uint8_t code) {
	if (code != SYS_SOC_OK) {
		errorNote = sys_socDetail();
	}
	return code;
}

static char lower(char c) {
	return (c >= 'A' && c <= 'Z') ? (char)(c + ('a' - 'A')) : c;
}

static uint8_t sameName(const char *a, const char *b) {
	while (*a != '\0' && *b != '\0') {
		if (lower(*a) != lower(*b)) {
			return FALSE;
		}
		a++;
		b++;
	}
	return (*a == '\0' && *b == '\0') ? TRUE : FALSE;
}

/* Splits in place on spaces and tabs. Returns the token count, or -1 if there are too many. */
static int tokenize(char *text, char *argv[]) {
	int argc = 0;
	char *p = text;

	while (*p != '\0') {
		while (*p == ' ' || *p == '\t') {
			*p = '\0';
			p++;
		}
		if (*p == '\0') {
			break;
		}
		if (argc >= (int)ARGV_MAX) {
			return -1;
		}
		argv[argc++] = p;
		while (*p != '\0' && *p != ' ' && *p != '\t') {
			p++;
		}
	}
	return argc;
}

static void execute(void) {
	char *argv[ARGV_MAX];
	int argc;
	int code;
	size_t i;
	uint8_t found = FALSE;

	line[length] = '\0';
	if (overflow) {
		reply("ERR line too long");
	} else {
		argc = tokenize(line, argv);
		if (argc < 0) {
			reply("ERR too many arguments");
		} else if (argc > 0) {
			for (i = 0U; i < COMMAND_COUNT && !found; i++) {
				if (sameName(argv[0], commands[i].name)) {
					found = TRUE;
					code = commands[i].handler(argc, argv);
					if (code != 0) {
						replyError(code);
					}
				}
			}
			if (!found) {
				reply("ERR unknown command");
			}
		}
	}
	length = 0U;
	overflow = FALSE;
	if (modeStep == NULL) {
		CLI_Print(PROMPT);
	}
}

static void handleByte(uint8_t c) {
	uint8_t last = previous;

	previous = c;
	if (c == '\r' || c == '\n') {
		if ((last == '\r' && c == '\n') || (last == '\n' && c == '\r')) {
			previous = 0U;
			return;
		}
		execute();
	} else if (c == 0x08U || c == 0x7FU) {
		if (!overflow && length > 0U) {
			length--;
		}
	} else if (c == '\t' || (c >= 0x20U && c <= 0x7EU)) {
		if (length < LINE_MAX_LEN) {
			line[length++] = (char)c;
		} else {
			overflow = TRUE;
		}
	}
}

static uint8_t settling = FALSE;
static sys_timer_t settleTimer = { .timerLengthMs = SETTLE_MS };

static uint8_t downloadStep(void) {
	int code;

	if (cc_state_get() != CC_STATE_OFF) {
		sys_xmodemAbort();
		sys_socAbort();
		replyError(1);
		return FALSE;
	}
	if (settling) {
		if (!sys_checkOutTimerMs(&settleTimer)) {
			return TRUE;
		}
		code = socResult(sys_socFinish());
	} else {
		switch (sys_xmodemStep()) {
		case SYS_XMODEM_RUNNING:
			return TRUE;
		case SYS_XMODEM_DONE:
			settling = TRUE;
			sys_timerInit(&settleTimer);
			return TRUE;
		case SYS_XMODEM_TIMEOUT:
			code = 2;
			break;
		case SYS_XMODEM_CANCELLED:
			code = 3;
			break;
		default:
			code = socResult(sys_socLastError());
			break;
		}
		sys_socAbort();
	}
	if (code == 0) {
		reply("OK");
	} else {
		replyError(code);
	}
	return FALSE;
}

static int cmdDownload(int argc, char *argv[]) {
	uint8_t code;

	(void)argc;
	(void)argv;
	if (cc_state_get() != CC_STATE_OFF) {
		return 1;
	}
	code = sys_socBegin();
	if (code != SYS_SOC_OK) {
		return socResult(code);
	}
	CLI_Print("Start XMODEM-CRC send within 60 s\r\n");
	settling = FALSE;
	sys_xmodemStart(sys_socBlock);
	CLI_SetMode(downloadStep);
	return 0;
}

static void printDay(uint8_t index, const sys_socDay_t *day) {
	char out[OUT_MAX];
	uint8_t len = 0U;

	append(out, &len, "  Day ");
	appendInt(out, &len, (int)index + 1);
	append(out, &len, ": ");
	appendInt(out, &len, day->year);
	appendChar(out, &len, '-');
	appendTwo(out, &len, day->month);
	appendChar(out, &len, '-');
	appendTwo(out, &len, day->day);
	appendChar(out, &len, ' ');
	appendTwo(out, &len, day->startHour);
	appendChar(out, &len, ':');
	appendTwo(out, &len, day->startMin);
	appendChar(out, &len, '-');
	appendTwo(out, &len, day->stopHour);
	appendChar(out, &len, ':');
	appendTwo(out, &len, day->stopMin);
	append(out, &len, " UTC\r\n");
	flush(out, len);
}

static int socInfo(void) {
	char out[OUT_MAX];
	uint8_t len = 0U;
	sys_socInfo_t info;
	uint8_t code = sys_socGetInfo(&info);
	uint8_t i;

	if (code == SYS_SOC_ERR_NONE) {
		reply("Active bank: none");
		return 0;
	}
	if (code != SYS_SOC_OK) {
		return socResult(code);
	}
	append(out, &len, "Active bank: ");
	appendChar(out, &len, (info.bank == SYS_SOC_BANK_A) ? 'A' : 'B');
	append(out, &len, " (record ");
	appendInt(out, &len, (int)info.record + 1);
	append(out, &len, " of ");
	appendInt(out, &len, (int)EEP_PTR_REC_COUNT);
	append(out, &len, ")\r\nFormat v");
	appendInt(out, &len, info.version);
	append(out, &len, ", ");
	appendInt(out, &len, info.days);
	append(out, &len, " days\r\n");
	flush(out, len);
	for (i = 0U; i < info.days; i++) {
		printDay(i, &info.day[i]);
	}
	return 0;
}

static int cmdSoc(int argc, char *argv[]) {
	if (argc == 2 && sameName(argv[1], "info")) {
		return socInfo();
	}
	if (argc == 2 && sameName(argv[1], "verify")) {
		uint8_t code;

		if (cc_state_get() != CC_STATE_OFF) {
			return 1;
		}
		code = sys_socVerify();
		if (code == SYS_SOC_OK) {
			reply("OK");
			return 0;
		}
		return socResult(code);
	}
	reply("usage: soc info | soc verify");
	return 1;
}

static int cmdPing(int argc, char *argv[]) {
	(void)argc;
	(void)argv;
	CLI_Print("pong\r\n");
	return 0;
}

static int cmdHelp(int argc, char *argv[]) {
	char out[OUT_MAX];
	uint8_t len;
	size_t width = 0U;
	size_t i;
	size_t n;

	(void)argc;
	(void)argv;
	for (i = 0U; i < COMMAND_COUNT; i++) {
		n = strlen(commands[i].name);
		if (n > width) {
			width = n;
		}
	}
	CLI_Print("Commands:\r\n");
	for (i = 0U; i < COMMAND_COUNT; i++) {
		len = 0U;
		append(out, &len, "  ");
		append(out, &len, commands[i].name);
		for (n = strlen(commands[i].name); n < width; n++) {
			appendChar(out, &len, ' ');
		}
		append(out, &len, "  ");
		append(out, &len, commands[i].help);
		append(out, &len, "\r\n");
		flush(out, len);
	}
	return 0;
}

uint8_t CLI_Init(void) {
	length = 0U;
	overflow = FALSE;
	previous = 0U;
	return sys_uartWrite((const uint8_t *)(BANNER PROMPT), (uint16_t)(sizeof(BANNER PROMPT) - 1U));
}

void CLI_Process(void) {
	uint8_t chunk[RX_CHUNK];
	uint16_t count;
	uint16_t i;

	if (modeStep != NULL) {
		if (!modeStep()) {
			modeStep = NULL;
			CLI_Print(PROMPT);
		}
		return;
	}
	count = sys_uartRead(chunk, RX_CHUNK);
	for (i = 0U; i < count && modeStep == NULL; i++) {
		handleByte(chunk[i]);
	}
}

void CLI_Print(const char *str) {
	if (str != NULL) {
		flush(str, (uint8_t)strlen(str));
	}
}

void CLI_SetMode(CLI_ModeStep step) {
	modeStep = step;
}

#else

uint8_t CLI_Init(void) {
	return TRUE;
}

void CLI_Process(void) {
}

void CLI_Print(const char *str) {
	(void)str;
}

void CLI_SetMode(CLI_ModeStep step) {
	(void)step;
}

#endif /* CLI_ENABLED */
