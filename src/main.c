#include <stdio.h>
#include <stdlib.h>

#include "config/config.h"
#include "daemon/daemon.h"
#include "logger/logger.h"
#include "path/path.h"

static void print_usage(const char *prog) {
    fprintf(stderr,
            "Usage: %s --repo <path> --time <HH:MM> "
            "[--branch <name>] [--tags] [--ssh-key <path>] "
            "[--log-file <path>] [--password <pass>]\n",
            prog);
}

int main(int argc, char **argv) {
    struct config cfg;
    char *resolved_repo = NULL;
    int ret = 0;

    if (argc <= 1) {
        print_usage(argv[0]);
        return 1;
    }
    if (config_init(&cfg) != 0) {
        fprintf(stderr, "Failed to initialize configuration\n");
        return 1;
    }
    if (config_parse(&cfg, argc, argv) != 0) {
        config_destroy(&cfg);
        return 1;
    }
    resolved_repo = path_resolve(cfg.repo_path);
    if (resolved_repo == NULL) {
        fprintf(stderr, "Error: unable to resolve path %s\n", cfg.repo_path);
        config_destroy(&cfg);
        return 1;
    }
    free(cfg.repo_path);
    cfg.repo_path = resolved_repo;
    if (!path_is_git_repo(cfg.repo_path)) {
        fprintf(stderr, "Error: %s is not a Git repository\n", cfg.repo_path);
        config_destroy(&cfg);
        return 1;
    }
    logger_init(cfg.log_file);
    ret = daemon_run(&cfg);
    logger_close();
    config_destroy(&cfg);
    return ret;
}
