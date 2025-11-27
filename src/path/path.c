// Path helper implementations.
#include "path.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

char *path_resolve(const char *path) {
    char *resolved;

    if (path == NULL) {
        return NULL;
    }
    resolved = realpath(path, NULL);
    return resolved;
}

int path_is_git_repo(const char *path) {
    struct stat st;
    size_t len;
    size_t full_len;
    int needs_slash;
    char *git_path;
    int is_repo = 0;

    if (path == NULL) {
        return 0;
    }
    len = strlen(path);
    needs_slash = (len == 0 || path[len - 1] != '/');
    full_len = len + (size_t)(needs_slash ? 5 : 4) + 1;
    git_path = malloc(full_len);
    if (git_path == NULL) {
        return 0;
    }
    if (needs_slash) {
        snprintf(git_path, full_len, "%s/.git", path);
    } else {
        snprintf(git_path, full_len, "%s.git", path);
    }
    if (stat(git_path, &st) == 0 && S_ISDIR(st.st_mode)) {
        is_repo = 1;
    }
    free(git_path);
    return is_repo;
}
