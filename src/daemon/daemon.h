#ifndef DAEMON_H
#define DAEMON_H

#include "../config/config.h"

int daemon_run(const struct config *cfg);
int git_push(const struct config *cfg);

#endif // ! DAEMON_H
