![Language](https://img.shields.io/badge/Language-C99-blue)
![Standard](https://img.shields.io/badge/Standard-POSIX-lightgrey)
![Build](https://img.shields.io/badge/Build-Make-blue)
![Status](https://img.shields.io/badge/Status-Finished-green)
![Maintained](https://img.shields.io/badge/Maintained-Yes-green)

# Auto-push

A small scheduled git pusher. You launch it with a repo path and a time (HH:MM); it resolves paths, starts an ssh-agent if needed, loads the provided key (optionally using your passphrase non-interactively), daemonizes, writes `daemon.pid` in the repo, and waits. When the scheduled time is reached it runs `git push` (with tags if requested) and then exits. Standard streams are closed unless you point logs to a file.

## Usage
- `--repo <path>`: git working tree to push (required).
- `--time <HH:MM>`: daily time to push (24h clock, required).
- `--branch <name>`: push a specific branch (`origin <name>`); otherwise pushes the current branch.
- `--tags`: include `--follow-tags` on push.
- `--ssh-key <path>`: identity file to use for SSH; path is resolved to an absolute path.
- `--password <pass>`: passphrase fed to `ssh-add` via a temporary askpass helper (avoids interactive prompts).
- `--log-file <path>`: append logs here; if omitted, the daemon closes stdin/stdout/stderr after forking.

Example:
```
cd src
./daemon_push --repo /path/to/repo --time 12:00 --branch main --tags \
  --ssh-key /path/to/id_ed25519 --password "my passphrase" --log-file /tmp/daemon.log
```

## Build and maintenance
Run these from `src/`:
- `make` (or `make daemon`): build `daemon_push`.
- `make clean`: remove objects and `daemon_push`.
