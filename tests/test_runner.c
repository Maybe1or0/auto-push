#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "../src/config/config.h"
#include "../src/date/date.h"
#include "../src/path/path.h"

struct test_case {
    const char *name;
    int (*fn)(void);
};

static int expect_fail(int line, const char *expr) {
    fprintf(stderr, "    failed at line %d: %s\n", line, expr);
    return 1;
}

#define EXPECT_TRUE(cond)                                                          \
    do {                                                                           \
        if (!(cond)) {                                                             \
            return expect_fail(__LINE__, #cond);                                   \
        }                                                                          \
    } while (0)

#define EXPECT_EQ_INT(a, b)                                                        \
    do {                                                                           \
        if ((a) != (b)) {                                                          \
            return expect_fail(__LINE__, #a " != " #b);                            \
        }                                                                          \
    } while (0)

#define EXPECT_STREQ(a, b)                                                         \
    do {                                                                           \
        if (strcmp((a), (b)) != 0) {                                               \
            return expect_fail(__LINE__, #a " != " #b);                            \
        }                                                                          \
    } while (0)

static char *make_temp_dir(void) {
    char tmpl[] = "/tmp/auto_push_testXXXXXX";
    char *dir = mkdtemp(tmpl);

    if (dir == NULL) {
        return NULL;
    }
    return strdup(dir);
}

static int test_date_parse_valid(void) {
    int hour = 0;
    int minute = 0;

    EXPECT_TRUE(date_parse_HHMM("00:00", &hour, &minute) == 0);
    EXPECT_EQ_INT(hour, 0);
    EXPECT_EQ_INT(minute, 0);

    EXPECT_TRUE(date_parse_HHMM("23:59", &hour, &minute) == 0);
    EXPECT_EQ_INT(hour, 23);
    EXPECT_EQ_INT(minute, 59);
    return 0;
}

static int test_date_parse_invalid(void) {
    int hour = 0;
    int minute = 0;

    EXPECT_TRUE(date_parse_HHMM("24:00", &hour, &minute) != 0);
    EXPECT_TRUE(date_parse_HHMM("12:60", &hour, &minute) != 0);
    EXPECT_TRUE(date_parse_HHMM("1:00", &hour, &minute) != 0);
    EXPECT_TRUE(date_parse_HHMM("abcd", &hour, &minute) != 0);
    EXPECT_TRUE(date_parse_HHMM("12345", &hour, &minute) != 0);
    return 0;
}

static int test_path_resolve_existing(void) {
    char *dir = make_temp_dir();
    char *resolved = NULL;
    struct stat st;

    EXPECT_TRUE(dir != NULL);
    EXPECT_TRUE(stat(dir, &st) == 0 && S_ISDIR(st.st_mode));

    resolved = path_resolve(dir);
    EXPECT_TRUE(resolved != NULL);
    EXPECT_TRUE(resolved[0] != '\0');

    free(resolved);
    rmdir(dir);
    free(dir);

    resolved = path_resolve("/this/does/not/exist/auto_push");
    EXPECT_TRUE(resolved == NULL);
    return 0;
}

static int test_path_is_git_repo(void) {
    char *dir = make_temp_dir();
    char git_dir[PATH_MAX];

    EXPECT_TRUE(dir != NULL);
    EXPECT_TRUE(path_is_git_repo(dir) == 0);

    snprintf(git_dir, sizeof(git_dir), "%s/.git", dir);
    EXPECT_TRUE(mkdir(git_dir, 0700) == 0);
    EXPECT_TRUE(path_is_git_repo(dir) == 1);

    rmdir(git_dir);
    rmdir(dir);
    free(dir);
    return 0;
}

static int test_config_parse_success(void) {
    struct config cfg;
    char *argv[] = {"prog",       "--repo",    "/tmp/repo",
                    "--time",     "12:34",     "--branch",
                    "main",       "--tags",    "--ssh-key",
                    "/tmp/key",   "--log-file", "/tmp/log",
                    "--password", "secret"};
    int argc = (int)(sizeof(argv) / sizeof(argv[0]));

    EXPECT_TRUE(config_init(&cfg) == 0);
    EXPECT_TRUE(config_parse(&cfg, argc, argv) == 0);
    EXPECT_STREQ(cfg.repo_path, "/tmp/repo");
    EXPECT_STREQ(cfg.time_str, "12:34");
    EXPECT_STREQ(cfg.branch, "main");
    EXPECT_STREQ(cfg.ssh_key, "/tmp/key");
    EXPECT_STREQ(cfg.log_file, "/tmp/log");
    EXPECT_STREQ(cfg.password, "secret");
    EXPECT_EQ_INT(cfg.use_tags, 1);

    config_destroy(&cfg);
    return 0;
}

static int test_config_missing_required(void) {
    struct config cfg;
    char *argv[] = {"prog", "--repo", "/tmp/repo"};
    int argc = (int)(sizeof(argv) / sizeof(argv[0]));
    int saved_err = dup(STDERR_FILENO);
    int devnull = open("/dev/null", O_WRONLY);

    EXPECT_TRUE(config_init(&cfg) == 0);
    if (devnull >= 0) {
        dup2(devnull, STDERR_FILENO);
    }
    EXPECT_TRUE(config_parse(&cfg, argc, argv) != 0);
    if (saved_err >= 0) {
        dup2(saved_err, STDERR_FILENO);
        close(saved_err);
    }
    if (devnull >= 0) {
        close(devnull);
    }
    config_destroy(&cfg);
    return 0;
}

static struct test_case tests[] = {
    {"date_parse_valid", test_date_parse_valid},
    {"date_parse_invalid", test_date_parse_invalid},
    {"path_resolve_existing", test_path_resolve_existing},
    {"path_is_git_repo", test_path_is_git_repo},
    {"config_parse_success", test_config_parse_success},
    {"config_missing_required", test_config_missing_required},
};

int main(void) {
    size_t i;
    int failed = 0;
    size_t total = sizeof(tests) / sizeof(tests[0]);

    for (i = 0; i < total; i++) {
        int rc = tests[i].fn();
        if (rc == 0) {
            printf("[PASS] %s\n", tests[i].name);
        } else {
            printf("[FAIL] %s\n", tests[i].name);
            failed++;
        }
    }
    printf("----\n");
    printf("Ran %zu tests: %zu passed, %d failed\n", total, total - failed, failed);
    return failed ? 1 : 0;
}
