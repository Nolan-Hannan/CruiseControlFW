/* UART command line. */

#include "cli.h"
#include "cc_control.h"
#include "sys_uart.h"
#include <stddef.h>
#include <string.h>

#if CLI_ENABLED

#define LINE_MAX_LEN 64U
#define ARGV_MAX     8U
#define RX_CHUNK     16U
#define OUT_MAX      96U

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
static int cmdTest(int argc, char *argv[]);

static const command_t commands[] = {
	{ "help", "list commands",                   cmdHelp },
	{ "ping", "reply pong",                      cmdPing },
	{ "test", "bench outputs: test stop to end", cmdTest },
};

#define COMMAND_COUNT (sizeof(commands) / sizeof(commands[0]))

static char line[LINE_MAX_LEN + 1U];
static uint8_t length = 0U;
static uint8_t overflow = FALSE;
static uint8_t previous = 0U;

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
	append(out, &len, "\r\n");
	flush(out, len);
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
	CLI_Print(PROMPT);
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

#define TEST_USAGE "usage: test speed 0|1 cm/s | led red|green|blue|off | dac mV | mux 0|1 | loop | stop"

/* Returns 0, 1 if text is not a number, or 2 if it is above max. */
static int parseUint(const char *text, uint32_t max, uint32_t *out) {
	uint32_t value = 0U;

	if (*text == '\0') {
		return 1;
	}
	for (; *text != '\0'; text++) {
		if (*text < '0' || *text > '9') {
			return 1;
		}
		value = value * 10U + (uint32_t)(*text - '0');
		if (value > max) {
			return 2;
		}
	}
	*out = value;
	return 0;
}

static int testUsage(void) {
	reply(TEST_USAGE);
	return 1;
}

static int testDone(int code) {
	if (code == 0) {
		reply("OK");
	}
	return code;
}

static void printLoop(const cc_loop_result_t *result) {
	char out[OUT_MAX];
	uint8_t len = 0U;

	append(out, &len, "raw ");
	appendInt(out, &len, result->raw_mv);
	append(out, &len, " mV, out ");
	appendInt(out, &len, result->out_mv);
	append(out, &len, " mV, loop ");
	appendInt(out, &len, result->loop_mv);
	append(out, &len, " mV\r\n");
	flush(out, len);
}

static int cmdTest(int argc, char *argv[]) {
	uint32_t first = 0U;
	uint32_t second = 0U;
	int status;

	if (argc == 2 && sameName(argv[1], "stop")) {
		cc_control_testStop();
		return testDone(0);
	}
	if (argc == 2 && sameName(argv[1], "loop")) {
		cc_loop_result_t result;
		int code = cc_control_testLoop(&result);

		if (code == CC_TEST_OK || code == CC_TEST_MISMATCH || code == CC_TEST_AMBIGUOUS) {
			printLoop(&result);
		}
		return testDone(code);
	}
	if (argc == 4 && sameName(argv[1], "speed")) {
		status = parseUint(argv[2], 1U, &first);
		if (status == 0) {
			status = parseUint(argv[3], 65535U, &second);
		}
		if (status != 0) {
			return (status == 2) ? 2 : testUsage();
		}
		return testDone(cc_control_testSpeed((uint8_t)first, (uint16_t)second));
	}
	if (argc == 3 && sameName(argv[1], "led")) {
		cc_led_t color;

		if (sameName(argv[2], "red")) {
			color = CC_LED_RED;
		} else if (sameName(argv[2], "green")) {
			color = CC_LED_GREEN;
		} else if (sameName(argv[2], "blue")) {
			color = CC_LED_BLUE;
		} else if (sameName(argv[2], "off")) {
			color = CC_LED_OFF;
		} else {
			return testUsage();
		}
		cc_control_testLed(color);
		return testDone(0);
	}
	if (argc == 3 && sameName(argv[1], "dac")) {
		status = parseUint(argv[2], 3300U, &first);
		if (status != 0) {
			return (status == 2) ? 2 : testUsage();
		}
		return testDone(cc_control_testDac((uint16_t)first));
	}
	if (argc == 3 && sameName(argv[1], "mux")) {
		status = parseUint(argv[2], 1U, &first);
		if (status != 0) {
			return (status == 2) ? 2 : testUsage();
		}
		return testDone(cc_control_testMux((uint8_t)first));
	}
	return testUsage();
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
	uint16_t count = sys_uartRead(chunk, RX_CHUNK);
	uint16_t i;

	for (i = 0U; i < count; i++) {
		handleByte(chunk[i]);
	}
}

void CLI_Print(const char *str) {
	if (str != NULL) {
		flush(str, (uint8_t)strlen(str));
	}
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

#endif /* CLI_ENABLED */
