// Logging implementation.
#include "logger.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

static FILE *log_stream = NULL;
static int log_to_file = 0;
static int log_can_close = 0;

static void logger_vlog(const char *level, const char *fmt, va_list ap) {
    time_t now = time(NULL);
    struct tm tm_now;
    char timestamp[32];

    if (log_stream == NULL) {
        log_stream = stderr;
    }
    if (localtime_r(&now, &tm_now) != NULL) {
        strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", &tm_now);
    } else {
        snprintf(timestamp, sizeof(timestamp), "unknown-time");
    }
    fprintf(log_stream, "[%s] %s: ", timestamp, level);
    vfprintf(log_stream, fmt, ap);
    fputc('\n', log_stream);
    fflush(log_stream);
}

void logger_init(const char *filepath) {
    FILE *fp = NULL;

    log_stream = NULL;
    log_to_file = 0;
    log_can_close = 0;
    if (filepath != NULL) {
        fp = fopen(filepath, "a");
    }
    if (fp != NULL) {
        log_stream = fp;
        log_to_file = 1;
        log_can_close = 1;
        return;
    }
    int dup_fd = dup(fileno(stderr));
    if (dup_fd >= 0) {
        fp = fdopen(dup_fd, "a");
        if (fp != NULL) {
            log_stream = fp;
            log_to_file = 0;
            log_can_close = 1;
            return;
        }
        close(dup_fd);
    }
    log_stream = stderr;
}

void logger_close(void) {
    if (log_can_close && log_stream != NULL) {
        fclose(log_stream);
    }
    log_stream = NULL;
    log_to_file = 0;
    log_can_close = 0;
}

void logger_info(const char *fmt, ...) {
    va_list ap;

    va_start(ap, fmt);
    logger_vlog("INFO", fmt, ap);
    va_end(ap);
}

void logger_error(const char *fmt, ...) {
    va_list ap;

    va_start(ap, fmt);
    logger_vlog("ERROR", fmt, ap);
    va_end(ap);
}
