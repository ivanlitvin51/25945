#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/resource.h>
#include <limits.h>
#include <string.h>

#define OPTS_CAPACITY 512

extern char **environ;

/* Запоминаем встреченную букву опции и ее аргумент */
struct CliOption {
    int letter;
    char *value;
};

int main(int argc, char *argv[]) {
    struct CliOption history[OPTS_CAPACITY];
    int total = 0;
    int opt;

    /* Читаем опции слева направо */
    while ((opt = getopt(argc, argv, "ispuU:cC:dvV:")) != -1) {
        if (opt == '?') {
            continue; /* getopt сам выводит ошибку о неизвестной букве */
        }

        if (total < OPTS_CAPACITY) {
            history[total].letter = opt;
            history[total].value = optarg;
            total++;
        }
    }

    /* Запускаем действия справа налево */
    for (int i = total - 1; i >= 0; i--) {
        char *arg = history[i].value;

        switch (history[i].letter) {
            case 'i': {
                printf("User IDs: real=%ld, effective=%ld | Group IDs: real=%ld, effective=%ld\n",
                       (long)getuid(), (long)geteuid(), (long)getgid(), (long)getegid());
                break;
            }

            case 's': {
                if (setpgid(0, 0) == 0) {
                    printf("Became group leader. PGID = %ld\n", (long)getpgrp());
                } else {
                    perror("setpgid");
                }
                break;
            }

            case 'p': {
                printf("PID: %ld, Parent PID: %ld, PGID: %ld\n",
                       (long)getpid(), (long)getppid(), (long)getpgrp());
                break;
            }

            case 'u': {
                struct rlimit rl;
                if (getrlimit(RLIMIT_NOFILE, &rl) == 0) {
                    printf("ulimit (file descriptors): soft=%lu, hard=%lu\n",
                           (unsigned long)rl.rlim_cur, (unsigned long)rl.rlim_max);
                } else {
                    perror("getrlimit NOFILE");
                }
                break;
            }

            case 'U': {
                long new_lim = atol(arg);
                if (new_lim <= 0) {
                    fprintf(stderr, "Invalid limit value: %s\n", arg);
                    break;
                }

                struct rlimit rl;
                if (getrlimit(RLIMIT_NOFILE, &rl) == 0) {
                    rl.rlim_cur = (rlim_t)new_lim;
                    if (setrlimit(RLIMIT_NOFILE, &rl) == 0) {
                        printf("ulimit set to %ld\n", new_lim);
                    } else {
                        perror("setrlimit NOFILE");
                    }
                }
                break;
            }

            case 'c': {
                struct rlimit rl;
                if (getrlimit(RLIMIT_CORE, &rl) == 0) {
                    printf("Core limit: soft=%lu bytes, hard=%lu bytes\n",
                           (unsigned long)rl.rlim_cur, (unsigned long)rl.rlim_max);
                } else {
                    perror("getrlimit CORE");
                }
                break;
            }

            case 'C': {
                long new_core = atol(arg);
                if (new_core < 0) {
                    fprintf(stderr, "Invalid core size: %s\n", arg);
                    break;
                }

                struct rlimit rl;
                if (getrlimit(RLIMIT_CORE, &rl) == 0) {
                    rl.rlim_cur = (rlim_t)new_core;
                    if (setrlimit(RLIMIT_CORE, &rl) == 0) {
                        printf("Core limit set to %ld bytes\n", new_core);
                    } else {
                        perror("setrlimit CORE");
                    }
                }
                break;
            }

            case 'd': {
                char path[PATH_MAX];
                if (getcwd(path, sizeof(path)) != NULL) {
                    printf("Current dir: %s\n", path);
                } else {
                    perror("getcwd");
                }
                break;
            }

            case 'v': {
                printf("--- Environment ---\n");
                for (char **env = environ; *env != NULL; env++) {
                    printf("%s\n", *env);
                }
                break;
            }

            case 'V': {
                if (strchr(arg, '=') == NULL) {
                    fprintf(stderr, "Error: -V needs format NAME=VALUE\n");
                } else if (putenv(arg) != 0) {
                    perror("putenv");
                } else {
                    printf("Set env: %s\n", arg);
                }
                break;
            }

            default:
                break;
        }
    }

    return 0;
}
