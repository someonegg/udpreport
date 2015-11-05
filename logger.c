
/*
 *  Copyright (C) someonegg .
 */

#include "logger.h"
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <fcntl.h>


struct log_s {
    int fd;
    int need_close_fd;
    int level;
};


#define LOG_MAXLINE          4096

static
int format_cur_time(char* buf, int buf_len)
{
    time_t ttNow;
    struct tm tmNow;

    ttNow = time(NULL);
    gmtime_r(&ttNow, &tmNow);

    return snprintf(
        buf, buf_len,
        "%04d%02d%02d-%02d:%02d:%02d\t",
        tmNow.tm_year + 1900,
        tmNow.tm_mon + 1,
        tmNow.tm_mday,
        tmNow.tm_hour,
        tmNow.tm_min,
        tmNow.tm_sec);
}

const char* LEVEL_HEADER[] = {"NONE \t", "FATAL\t", "ERROR\t", "WARN \t", "INFO \t", "DEBUG\t"};


log_t* log_create()
{
    log_t* log = (log_t*)malloc( sizeof(log_t) );
    if (log == NULL)
        return NULL;

    log->fd            = STDERR_FILENO;
    log->need_close_fd = 0;
    log->level         = LOG_LEVEL_WARN;

    return log;
}


void log_destroy(log_t* log)
{
    if (log->need_close_fd)
        close(log->fd);

    free(log);
}


int  log_output_to(log_t* log, const char* file)
{
    int fd;

    fd = open(file, O_WRONLY | O_CREAT | O_APPEND, S_IRWXU);
    if (fd == -1)
        return -1;

    if (log->need_close_fd)
        close(log->fd);

    log->fd            = fd;
    log->need_close_fd = 1;

    return 0;
}

void log_set_level(log_t* log, int level)
{
    if (level < LOG_LEVEL_NONE)
        log->level = LOG_LEVEL_NONE;
    else if(level > LOG_LEVEL_DEBUG)
        log->level = LOG_LEVEL_DEBUG;
    else
        log->level = level;
}


void log_write(log_t* log, int level, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    log_write_v(log, level, fmt, args);
    va_end(args);
}

void log_write_v(log_t* log, int level, const char *fmt, va_list args)
{
    char  out_buf[LOG_MAXLINE];
    char* cur_buf = out_buf;
    int   avail_len = LOG_MAXLINE;
    int   ret_len;

    if (level < LOG_LEVEL_NONE)
        level = LOG_LEVEL_NONE;
    else if(level > LOG_LEVEL_DEBUG)
        level = LOG_LEVEL_DEBUG;

    if (level > log->level)
        return;

    ret_len = format_cur_time(cur_buf, avail_len);
    if (ret_len < 0 || ret_len >= avail_len)
        return;
    cur_buf = cur_buf + ret_len;
    avail_len = avail_len - ret_len;

    ret_len = snprintf(cur_buf, avail_len, "%s", LEVEL_HEADER[level]);
    if (ret_len < 0 || ret_len >= avail_len)
        return;
    cur_buf = cur_buf + ret_len;
    avail_len = avail_len - ret_len;

    ret_len = vsnprintf(cur_buf, avail_len, fmt, args);
    if (ret_len < 0 || ret_len >= avail_len) {
        // truncated
        ret_len = avail_len - 1;
    }

    cur_buf = cur_buf + ret_len;
    avail_len = avail_len - ret_len;

    cur_buf[0] = '\n';
    avail_len -= 1;

    write(log->fd, out_buf, sizeof(out_buf) - avail_len);
}


log_t default_log ={STDERR_FILENO, 0, LOG_LEVEL_WARN};

int  DEFAULT_LOG_OUTPUT_TO(const char* file)
{
    return log_output_to(&default_log, file);
}

void DEFAULT_LOG_SET_LEVEL(int level)
{
    log_set_level(&default_log, level);
}

void DEFAULT_LOG_WRITE(int level, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    log_write_v(&default_log, level, fmt, args);
    va_end(args);
}

void DEFAULT_LOG_WRITE_V(int level, const char *fmt, va_list args)
{
    log_write_v(&default_log, level, fmt, args);
}

