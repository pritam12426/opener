#ifndef _LOG_H_
#define _LOG_H_


#ifdef __cplusplus
extern "C" {
#endif

/* --------------------------------------------------
 * Log levels
 * -------------------------------------------------- */
typedef enum {
	LOG_LEVEL_ERROR = 0,
	LOG_LEVEL_WARN  = 1,
	LOG_LEVEL_INFO  = 2,
	LOG_LEVEL_DEBUG = 3
} Log_level_t;

/* --------------------------------------------------
 * Logger configuration
 * -------------------------------------------------- */
void        log_set_level(Log_level_t level);
Log_level_t log_get_level(void);

/* --------------------------------------------------
 * Internal implementation
 * Do not call directly.
 * -------------------------------------------------- */
void log_record(
	Log_level_t level,
	const char *file,
	int         line,
	const char *func,
	const char *fmt,
	...
);

/* --------------------------------------------------
 * Public logging macros
 * Do NOT append '\n'
 * -------------------------------------------------- */

#ifdef LOG_SHOW_SOURCE_LOCATION

#define LOG_ERROR(...) \
	log_record(LOG_LEVEL_ERROR, __FILE__, __LINE__, __func__, __VA_ARGS__)

#define LOG_WARN(...) \
	log_record(LOG_LEVEL_WARN, __FILE__, __LINE__, __func__, __VA_ARGS__)

#define LOG_INFO(...) \
	log_record(LOG_LEVEL_INFO, __FILE__, __LINE__, __func__, __VA_ARGS__)

#define LOG_DEBUG(...) \
	log_record(LOG_LEVEL_DEBUG, __FILE__, __LINE__, __func__, __VA_ARGS__)

#else

#define LOG_ERROR(...) \
	log_record(LOG_LEVEL_ERROR, 0, 0, 0, __VA_ARGS__)

#define LOG_WARN(...) \
	log_record(LOG_LEVEL_WARN, 0, 0, 0, __VA_ARGS__)

#define LOG_INFO(...) \
	log_record(LOG_LEVEL_INFO, 0, 0, 0, __VA_ARGS__)

#define LOG_DEBUG(...) \
	log_record(LOG_LEVEL_DEBUG, 0, 0, 0, __VA_ARGS__)

#endif /* LOG_SHOW_SOURCE_LOCATION */

#ifdef __cplusplus
}
#endif


#endif  // _LOG_H_
