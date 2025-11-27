// Simple logger for the daemon.
#ifndef LOGGER_H
#define LOGGER_H

void logger_init(const char *filepath);
void logger_close(void);
void logger_info(const char *fmt, ...);
void logger_error(const char *fmt, ...);

#endif
