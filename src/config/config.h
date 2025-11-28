#ifndef CONFIG_H
#define CONFIG_H

struct config {
    char *repo_path;
    char *time_str;     // HH:MM
    char *branch;       // nullable
    char *ssh_key;      // nullable
    char *log_file;     // nullable
    char *password;     // nullable, passphrase for ssh-add
    int use_tags;       // boolean
};

int config_init(struct config *cfg);
int config_parse(struct config *cfg, int argc, char **argv);
void config_destroy(struct config *cfg);

#endif
