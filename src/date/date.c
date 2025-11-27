#include "date.h"

#include <ctype.h>
#include <string.h>
#include <time.h>

int date_parse_HHMM(const char *s, int *hour, int *minute) {
    if (s == NULL || hour == NULL || minute == NULL) {
        return 1;
    }
    if (strlen(s) != 5 || s[2] != ':') {
        return 1;
    }
    if (!isdigit((unsigned char)s[0]) || !isdigit((unsigned char)s[1]) ||
        !isdigit((unsigned char)s[3]) || !isdigit((unsigned char)s[4])) {
        return 1;
    }
    *hour = (s[0] - '0') * 10 + (s[1] - '0');
    *minute = (s[3] - '0') * 10 + (s[4] - '0');
    if (*hour < 0 || *hour > 23 || *minute < 0 || *minute > 59) {
        return 1;
    }
    return 0;
}

int date_is_now(int hour, int minute) {
    time_t now = time(NULL);
    struct tm tm_now;

    if (localtime_r(&now, &tm_now) == NULL) {
        return 0;
    }
    if (tm_now.tm_hour == hour && tm_now.tm_min == minute) {
        return 1;
    }
    return 0;
}
