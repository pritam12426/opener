#include "log.h"

#include <stdarg.h>
#include <stdio.h>
#include <unistd.h> /* isatty(), fileno() */


#define COLOR_RESET        "\x1b[0m"
#define COLOR_BOLD_RED     "\x1b[1;31m"
#define COLOR_BOLD_GREEN   "\x1b[1;32m"
#define COLOR_BOLD_YELLOW  "\x1b[1;33m"
#define COLOR_BOLD_BLUE    "\x1b[1;34m"
#define COLOR_BOLD_MAGENTA "\x1b[1;35m"
#define COLOR_BOLD_CYAN    "\x1b[1;36m"
#define COLOR_DIM          "\x1b[2m"


static Log_level_t g_log_level = LOG_LEVEL_INFO;


static void default_log_handler(Log_level_t level)
{
	switch (level) {
	case LOG_LEVEL_INFO:
		fprintf(stderr, "[INFO ] ");
		break;
	case LOG_LEVEL_WARN:
		fprintf(stderr, "[WARN ] ");
		break;
	case LOG_LEVEL_ERROR:
		fprintf(stderr, "[ERROR] ");
		break;
	case LOG_LEVEL_DEBUG:
		fprintf(stderr, "[DEBUG] ");
		break;
	}
}

static void use_color_log_handler(Log_level_t level)
{
	switch (level) {
	case LOG_LEVEL_ERROR:
		fprintf(stderr, "🚨 [" COLOR_BOLD_RED "ERRO " COLOR_RESET "] ");
		break;
	case LOG_LEVEL_WARN:
		fprintf(stderr, "⚠️  [" COLOR_BOLD_YELLOW "WARN " COLOR_RESET "] ");
		break;
	case LOG_LEVEL_INFO:
		fprintf(stderr, "ℹ️  [" COLOR_BOLD_GREEN "INFO " COLOR_RESET "] ");
		break;
	case LOG_LEVEL_DEBUG:
		fprintf(stderr, "🛠️  [" COLOR_BOLD_CYAN "DEBUG" COLOR_RESET "] ");
		break;
	default:
		fprintf(stderr, "[UNKWN] ");
		break;
	}
}


void log_record(Log_level_t level,
                 const char *file,
                 int         line,
                 const char *func,
                 const char *fmt,
                 ...)
{
	int use_color = 0;
	if (level > g_log_level) return;

	if (isatty(fileno(stderr))) {
		use_color = 1;
		use_color_log_handler(level);
	}
	else default_log_handler(level);

	if (file) {
		fprintf(stderr,
			"%s[%s:%d:%s]%s ",
			use_color ? COLOR_DIM : "",
			file,
			line,
			func,
			use_color ? COLOR_RESET : "");
	}

	va_list args;
	va_start(args, fmt);
	vfprintf(stderr, fmt, args);
	va_end(args);

	fputc('\n', stderr);
}


void log_set_level(Log_level_t level)
{
	g_log_level = level;
}


Log_level_t log_get_level(void)
{
	return g_log_level;
}
