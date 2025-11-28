#include "daemon.h"

#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "../date/date.h"
#include "../logger/logger.h"

static int parse_agent_output(const char *out, char **sock_out, char **pid_out) {
    char *copy;
    char *token;
    char *saveptr = NULL;

    *sock_out = NULL;
    *pid_out = NULL;
    copy = strdup(out);
    if (copy == NULL) {
        return 1;
    }
    token = strtok_r(copy, ";\n", &saveptr);
    while (token != NULL) {
        while (*token == ' ' || *token == '\t') {
            token++;
        }
        if (strncmp(token, "SSH_AUTH_SOCK=", 14) == 0) {
            free(*sock_out);
            *sock_out = strdup(token + 14);
        } else if (strncmp(token, "SSH_AGENT_PID=", 14) == 0) {
            free(*pid_out);
            *pid_out = strdup(token + 14);
        }
        token = strtok_r(NULL, ";\n", &saveptr);
    }
    free(copy);
    if (*sock_out == NULL || *pid_out == NULL) {
        free(*sock_out);
        free(*pid_out);
        *sock_out = NULL;
        *pid_out = NULL;
        return 1;
    }
    return 0;
}

static int start_ssh_agent_if_needed(void) {
    const char *sock = getenv("SSH_AUTH_SOCK");
    FILE *fp = NULL;
    char buffer[512];
    size_t len;
    int status;
    char *sock_value = NULL;
    char *pid_value = NULL;

    if (sock != NULL && sock[0] != '\0') {
        return 0;
    }
    fp = popen("ssh-agent -s", "r");
    if (fp == NULL) {
        logger_error("Failed to start ssh-agent");
        return 1;
    }
    len = fread(buffer, 1, sizeof(buffer) - 1, fp);
    buffer[len] = '\0';
    status = pclose(fp);
    if (len == 0 || status == -1 || !WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        logger_error("ssh-agent command failed");
        return 1;
    }
    if (parse_agent_output(buffer, &sock_value, &pid_value) != 0) {
        logger_error("Failed to parse ssh-agent output");
        return 1;
    }
    if (setenv("SSH_AUTH_SOCK", sock_value, 1) != 0 ||
        setenv("SSH_AGENT_PID", pid_value, 1) != 0) {
        free(sock_value);
        free(pid_value);
        logger_error("Failed to export ssh-agent environment");
        return 1;
    }
    logger_info("Started ssh-agent (PID %s)", pid_value);
    free(sock_value);
    free(pid_value);
    return 0;
}

static int create_askpass_script(char **path_out) {
    char tmpl[] = "/tmp/auto_push_askpassXXXXXX";
    int fd;
    const char script[] = "#!/bin/sh\nprintf \"%s\" \"$SSH_KEY_PASSPHRASE\"\n";

    *path_out = NULL;
    fd = mkstemp(tmpl);
    if (fd < 0) {
        return 1;
    }
    if (write(fd, script, sizeof(script) - 1) != (ssize_t)(sizeof(script) - 1)) {
        close(fd);
        unlink(tmpl);
        return 1;
    }
    if (fchmod(fd, 0700) != 0) {
        close(fd);
        unlink(tmpl);
        return 1;
    }
    close(fd);
    *path_out = strdup(tmpl);
    if (*path_out == NULL) {
        unlink(tmpl);
        return 1;
    }
    return 0;
}

static int add_key_to_agent(const struct config *cfg) {
    pid_t pid;
    int status;
    char *askpass_path = NULL;
    int have_askpass = 0;
    int idx = 0;
    char *argv[6];

    if (cfg == NULL || cfg->ssh_key == NULL) {
        logger_info("No ssh key provided; skipping ssh-add");
        return 0;
    }
    if (cfg->password != NULL) {
        if (create_askpass_script(&askpass_path) != 0) {
            logger_error("Failed to create askpass helper for ssh-add");
            return 1;
        }
        have_askpass = 1;
    }
    pid = fork();
    if (pid < 0) {
        logger_error("Failed to fork for ssh-add");
        if (have_askpass) {
            unlink(askpass_path);
            free(askpass_path);
        }
        return 1;
    }
    if (pid == 0) {
        if (have_askpass) {
            setenv("SSH_ASKPASS", askpass_path, 1);
            setenv("SSH_ASKPASS_REQUIRE", "force", 1);
            setenv("SSH_KEY_PASSPHRASE", cfg->password, 1);
            if (getenv("DISPLAY") == NULL) {
                setenv("DISPLAY", ":0", 1);
            }
        }
        argv[idx++] = "ssh-add";
#ifdef __APPLE__
        argv[idx++] = "--apple-use-keychain";
#endif
        argv[idx++] = (char *)cfg->ssh_key;
        argv[idx] = NULL;
        execvp("ssh-add", argv);
        _exit(127);
    }
    if (waitpid(pid, &status, 0) < 0) {
        logger_error("ssh-add waitpid failed");
        status = -1;
    }
    if (have_askpass) {
        unlink(askpass_path);
        free(askpass_path);
    }
    if (status == -1) {
        return 1;
    }
    if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
        logger_info("ssh-add loaded key %s", cfg->ssh_key);
        return 0;
    }
    if (WIFEXITED(status)) {
        logger_error("ssh-add failed with exit code %d", WEXITSTATUS(status));
    } else if (WIFSIGNALED(status)) {
        logger_error("ssh-add terminated by signal %d", WTERMSIG(status));
    } else {
        logger_error("ssh-add failed (status=%d)", status);
    }
    return 1;
}

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

int ssh_agent_prepare(const struct config *cfg) {
    if (cfg == NULL) {
        return 1;
    }
    if (start_ssh_agent_if_needed() != 0) {
        return 1;
    }
    if (add_key_to_agent(cfg) != 0) {
        return 1;
    }
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
            if (git_push(cfg) == 0) {
                logger_info("Push finished, shutting down daemon");
                return 0;
            }
            logger_error("Push failed, shutting down daemon");
            return 1;
        } else {
            sleep(1);
        }
    }
    return 0;
}
