CC      ?= cc
CFLAGS  ?= -Wall -Wextra -std=c99

SRCS    := main.c \
           config/config.c \
           daemon/daemon.c \
           date/date.c \
           logger/logger.c \
           path/path.c
OBJS    := $(SRCS:.c=.o)
BIN     := daemon_push

.PHONY: all daemon clean

all: daemon

daemon: $(BIN)

$(BIN): $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o $(BIN)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(BIN)