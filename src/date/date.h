#ifndef DATE_H
#define DATE_H

int date_parse_HHMM(const char *s, int *hour, int *minute);
int date_is_now(int hour, int minute);

#endif
