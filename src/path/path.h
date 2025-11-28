#ifndef PATH_H
#define PATH_H

char *path_resolve(const char *path);
int path_is_git_repo(const char *path);

#endif
