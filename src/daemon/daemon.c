#include "daemon.h"

#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "../date/date.h"
#include "../logger/logger.h"

static void close_standard_streams(void) {
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);
}

static int write_pid_file(const char *repo_path, pid_t pid) {
    size_t len;
    int need_slash;
    size_t total;
    char *path;
    FILE *fp;

    if (repo_path == NULL) {
        return 1;
    }
    len = strlen(repo_path);
    need_slash = (len == 0 || repo_path[len - 1] != '/');
    total = len + (size_t)(need_slash ? 12 : 11) + 1;
    path = malloc(total);
    if (path == NULL) {
        return 1;
    }
    if (need_slash) {
        snprintf(path, total, "%s/daemon.pid", repo_path);
    } else {
        snprintf(path, total, "%sdaemon.pid", repo_path);
    }
    fp = fopen(path, "w");
    if (fp == NULL) {
        free(path);
        return 1;
    }
    fprintf(fp, "%d\n", (int)pid);
    fclose(fp);
    free(path);
    return 0;
}

static int build_ssh_command(const char *key_path) {
    char *cmd;
    size_t len;

    if (key_path == NULL) {
        return 0;
    }
    len = strlen(key_path) + strlen("ssh -i ") + 1;
    cmd = malloc(len);
    if (cmd == NULL) {
        return 1;
    }
    snprintf(cmd, len, "ssh -i %s", key_path);
    if (setenv("SSH_COMMAND", cmd, 1) != 0) {
        free(cmd);
        return 1;
    }
    free(cmd);
    return 0;
}

int git_push(const struct config *cfg) {
    pid_t pid;
    int status;
    int idx = 0;
    char *argv[8];

    if (cfg == NULL || cfg->repo_path == NULL) {
        return 1;
    }
    pid = fork();
    if (pid < 0) {
        logger_error("Failed to fork for git push");
        return 1;
    }
    if (pid == 0) {
        if (cfg->ssh_key != NULL && build_ssh_command(cfg->ssh_key) != 0) {
            _exit(127);
        }
        argv[idx++] = "git";
        argv[idx++] = "-C";
        argv[idx++] = (char *)cfg->repo_path;
        argv[idx++] = "push";
        if (cfg->use_tags) {
            argv[idx++] = "--follow-tags";
        }
        if (cfg->branch != NULL) {
            argv[idx++] = "origin";
            argv[idx++] = (char *)cfg->branch;
        }
        argv[idx] = NULL;
        execvp("git", argv);
        _exit(127);
    }
    if (waitpid(pid, &status, 0) < 0) {
        logger_error("git push waitpid failed");
        return 1;
    }
    if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
        logger_info("git push completed successfully");
        return 0;
    }
    if (WIFEXITED(status)) {
        logger_error("git push failed with exit code %d", WEXITSTATUS(status));
    } else if (WIFSIGNALED(status)) {
        logger_error("git push terminated by signal %d", WTERMSIG(status));
    } else {
        logger_error("git push failed (status=%d)", status);
    }
    return 1;
}

int daemon_run(const struct config *cfg) {
    int hour = 0;
    int minute = 0;
    pid_t pid;

    if (cfg == NULL) {
        return 1;
    }
    if (date_parse_HHMM(cfg->time_str, &hour, &minute) != 0) {
        logger_error("Invalid time format: %s", cfg->time_str);
        return 1;
    }
    pid = fork();
    if (pid < 0) {
        logger_error("Failed to fork daemon");
        return 1;
    }
    if (pid > 0) {
        return 0;
    }
    if (setsid() < 0) {
        _exit(1);
    }
    signal(SIGHUP, SIG_IGN);
    if (chdir("/") != 0) {
        _exit(1);
    }
    if (write_pid_file(cfg->repo_path, getpid()) != 0) {
        logger_error("Failed to write PID file");
    }
    if (cfg->log_file == NULL) {
        close_standard_streams();
    }
    logger_info("Daemon started: repo=%s time=%s branch=%s tags=%d",
                cfg->repo_path, cfg->time_str,
                cfg->branch != NULL ? cfg->branch : "(current)", cfg->use_tags);
    for (;;) {
        if (date_is_now(hour, minute)) {
            logger_info("Scheduled time reached, running git push");
            git_push(cfg);
            sleep(60);
        } else {
            sleep(1);
        }
    }
    return 0;
}
