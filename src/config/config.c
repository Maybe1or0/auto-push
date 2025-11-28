#include "config.h"

#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../date/date.h"

enum option_values {
    OPT_REPO = 1,
    OPT_TIME,
    OPT_BRANCH,
    OPT_TAGS,
    OPT_SSH_KEY,
    OPT_LOG_FILE,
    OPT_PASSWORD
};

static int set_string(char **dst, const char *src) {
    char *copy;

    if (src == NULL) {
        return 1;
    }
    copy = strdup(src);
    if (copy == NULL) {
        return 1;
    }
    free(*dst);
    *dst = copy;
    return 0;
}

int config_init(struct config *cfg) {
    if (cfg == NULL) {
        return 1;
    }
    cfg->repo_path = NULL;
    cfg->time_str = NULL;
    cfg->branch = NULL;
    cfg->ssh_key = NULL;
    cfg->log_file = NULL;
    cfg->password = NULL;
    cfg->use_tags = 0;
    return 0;
}

static int parse_option(struct config *cfg, int opt, const char *arg) {
    switch (opt) {
    case OPT_REPO:
        return set_string(&cfg->repo_path, arg);
    case OPT_TIME:
        return set_string(&cfg->time_str, arg);
    case OPT_BRANCH:
        return set_string(&cfg->branch, arg);
    case OPT_SSH_KEY:
        return set_string(&cfg->ssh_key, arg);
    case OPT_LOG_FILE:
        return set_string(&cfg->log_file, arg);
    case OPT_TAGS:
        cfg->use_tags = 1;
        return 0;
    case OPT_PASSWORD:
        return set_string(&cfg->password, arg);
    default:
        return 1;
    }
}

int config_parse(struct config *cfg, int argc, char **argv) {
    int opt;
    int hour = 0;
    int minute = 0;
    const struct option options[] = {
        {"repo", required_argument, NULL, OPT_REPO},
        {"time", required_argument, NULL, OPT_TIME},
        {"branch", required_argument, NULL, OPT_BRANCH},
        {"tags", no_argument, NULL, OPT_TAGS},
        {"ssh-key", required_argument, NULL, OPT_SSH_KEY},
        {"log-file", required_argument, NULL, OPT_LOG_FILE},
        {"password", required_argument, NULL, OPT_PASSWORD},
        {0, 0, 0, 0}};

    if (cfg == NULL) {
        return 1;
    }
    optind = 1;
    while ((opt = getopt_long(argc, argv, "", options, NULL)) != -1) {
        if (parse_option(cfg, opt, optarg) != 0) {
            fprintf(stderr, "Error: failed to parse option\n");
            return 1;
        }
    }
    if (cfg->repo_path == NULL || cfg->time_str == NULL) {
        fprintf(stderr, "Error: --repo and --time are required\n");
        return 1;
    }
    if (optind < argc) {
        fprintf(stderr, "Error: unexpected argument: %s\n", argv[optind]);
        return 1;
    }
    if (date_parse_HHMM(cfg->time_str, &hour, &minute) != 0) {
        fprintf(stderr, "Error: invalid time format, expected HH:MM\n");
        return 1;
    }
    return 0;
}

void config_destroy(struct config *cfg) {
    if (cfg == NULL) {
        return;
    }
    free(cfg->repo_path);
    free(cfg->time_str);
    free(cfg->branch);
    free(cfg->ssh_key);
    free(cfg->log_file);
    free(cfg->password);
    cfg->repo_path = NULL;
    cfg->time_str = NULL;
    cfg->branch = NULL;
    cfg->ssh_key = NULL;
    cfg->log_file = NULL;
    cfg->password = NULL;
    cfg->use_tags = 0;
}
