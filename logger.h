
/*
 *  Copyright (C) someonegg .
 */

#ifndef __LOGGER_H__
#define __LOGGER_H__

#include <stdarg.h>


#define LOG_LEVEL_NONE       0
#define LOG_LEVEL_FATAL      1
#define LOG_LEVEL_ERROR      2
#define LOG_LEVEL_WARN       3
#define LOG_LEVEL_INFO       4
#define LOG_LEVEL_DEBUG      5


typedef struct log_s log_t;


/*
 *  By default, the log will be output to the stderr, you can change it
 *  by call log_output_to().
 *  The default log level is LOG_LEVEL_WARN, you can change it
 *  by call log_set_level().
 *
 *  RETURN VALUE
 *    log_create() return a log_t pointer on success. Otherwise, NULL is returned.
 */
log_t* log_create();

/*
 *  Always successful.
 */
void log_destroy(log_t* log);


/*
 *  The caller must be able to open the file with open(O_WRONLY | O_APPEND).
 *
 *  RETURN VALUE
 *    log_output_to() returns zero on success. Otherwise, -1 is returned.
 */
int  log_output_to(log_t* log, const char* file);

/*
 *  Always successful
 */
void log_set_level(log_t* log, int level);


void log_write(log_t* log, int level, const char *fmt, ...);

void log_write_v(log_t* log, int level, const char *fmt, va_list args);


/*
 *  Default Log
 */

int  DEFAULT_LOG_OUTPUT_TO(const char* file);

void DEFAULT_LOG_SET_LEVEL(int level);

void DEFAULT_LOG_WRITE(int level, const char *fmt, ...);

void DEFAULT_LOG_WRITE_V(int level, const char *fmt, va_list args);


#endif // __LOGGER_H__
