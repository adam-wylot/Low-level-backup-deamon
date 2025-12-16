#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE

#include "sighandler.h"
#include "consoleio.h"
#include <sys/stat.h>
#include <stdio.h>
#include <errno.h>
#include <sys/types.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>
#include <ftw.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>

#define COPY_BLOCKSIZE 4096


int dir_exists(const char *path) {
    struct stat st;

    if (stat(path, &st) == 0) {
        if (S_ISDIR(st.st_mode)) {
            return 1;
        }

        // ! To nie katalog
        return -3;
    }

    if (errno == ENOENT) {
        // ? katalog nie istnieje
        return 0;
    }
    if (errno == EACCES) {
        // ! Brak uprawnień
        return -2;
    }

    // ! Jakis błąd (errno)
    return -1;
}

int mkdir_recursive(const char *path, mode_t mode) {
    char *dup = NULL;
    char *p = NULL;

    dup = strdup(path);
    if (dup == NULL) {
        // ! Błąd pamięci
        return -1;
    }

    // iteracja po segmentach ścieżki
    // zaczynamy od +1 elementu, aby pominąć / w ścieżce absolutnej
    for (p = dup + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';

            errno = 0;
            if (mkdir(dup, mode) != 0) {
                if (errno == EACCES) {
                    // ! Brak uprawnień
                    free(dup);
                    return -2;
                }
                if (errno != EEXIST) {
                    // ! Nieokreślony błąd
                    free(dup);
                    return -1;
                }
            }

            *p = '/';
        }
    }

    // ? Ostatni katalog (pętla nie bierze go pod uwagę)
    errno = 0;
    if (mkdir(dup, mode) != 0) {
        if (errno == EACCES) {
            // ! Brak uprawnień
            free(dup);
            return -2;
        }
        if (errno != EEXIST) {
            // ! Nieokreślony błąd
            free(dup);
            return -1;
        }
    }

    free(dup);
    return 0;
}

int is_dir_empty(const char *path) {
    DIR *dir = NULL;
    struct dirent *entry;
    int found_file = 0;

    // ? otwarcie katalogu
    errno = 0;

    dir = opendir(path);
    if (dir == NULL) {
        if (errno == EACCES) {
            // ! Brak uprawnień
            return -2;
        }

        return -1;
    }

    // ? iteracja po wpisach
    while ((entry = readdir(dir)) != NULL) {
        // pomijamy symlinki na nadrzędny katalog i na samego siebie
        if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
            // ! znaleziono jakiś wpis
            found_file = 1;
            break;
        }
    }

    // ? Zwolnienie zasobów i wynik
    closedir(dir);

    if (found_file) {
        return 0;
    }

    return 1;
}

int is_subdirectory(const char *parent_path, const char *path) {
    // ? Porównanie
    size_t parentpath_len = strlen(parent_path);
    size_t path_len = strlen(path);

    if (strncmp(parent_path, path, parentpath_len) == 0) {
        if (parentpath_len == path_len) {
            // to ten sam katalog
            return 1;
        }

        if (parentpath_len < path_len && path[parentpath_len] == '/') {
            // podkatalog
            return 1;
        }
    }

    return 0;
}

int copy_file(const char *src, const char *dest) {
    // ! Chwilowe zablokowanie sygnałów — sekcja krytyczna
    sigset_t mask, oldmask;
    sigemptyset(&mask);

    sigaddset(&mask, SIGINT);
    sigaddset(&mask, SIGTERM);

    if (sigprocmask(SIG_BLOCK, &mask, &oldmask) == -1) {
        return -1;
    }

    // ? Plik źródłowy
    int fd_from = 0;
    int fd_to = 0;
    struct stat stat_buf = {0};

    // ? Sprawdzenie co to za plik
    if (lstat(src, &stat_buf) < 0) {
        if (errno == EACCES) {
            if (sigprocmask(SIG_SETMASK, &oldmask, NULL) == -1) {
                return -1;
            }

            return 0;
        }

        sigprocmask(SIG_SETMASK, &oldmask, NULL);
        return -1;
    }

    // Kopiujemy tylko zwykłe pliki (pomiń FIFO, SOCK, BLK itp.)
    if (!S_ISREG(stat_buf.st_mode)) {
        if (sigprocmask(SIG_SETMASK, &oldmask, NULL) == -1) {
            return -1;
        }

        return 0;
    }

    // ? Otworzenie, jeżeli to rzeczywiście plik
    if ((fd_from = open(src, O_RDONLY)) < 0) {
        if (errno == EACCES) {
            if (sigprocmask(SIG_SETMASK, &oldmask, NULL) == -1) {
                return -1;
            }

            return 0;
        }

        sigprocmask(SIG_SETMASK, &oldmask, NULL);
        return -1;
    }

    // ? Plik docelowy
    unlink(dest);

    if ((fd_to = TEMP_FAILURE_RETRY(open(dest, O_WRONLY | O_CREAT | O_TRUNC, stat_buf.st_mode))) < 0) {
        if (errno == EACCES) {
            TEMP_FAILURE_RETRY(close(fd_from));
            if (sigprocmask(SIG_SETMASK, &oldmask, NULL) == -1) {
                return -1;
            }

            return 0;
        }

        TEMP_FAILURE_RETRY(close(fd_from));
        sigprocmask(SIG_SETMASK, &oldmask, NULL);
        return -1;
    }

    // ? Kopiowanie zawartości
    char buf[COPY_BLOCKSIZE] = "";
    ssize_t read_count = 0;

    while ((read_count = bulk_read(fd_from, buf, COPY_BLOCKSIZE)) > 0) {
        if (bulk_write2(fd_to, buf, read_count) != read_count) {
            TEMP_FAILURE_RETRY(close(fd_from));
            TEMP_FAILURE_RETRY(close(fd_to));
            sigprocmask(SIG_SETMASK, &oldmask, NULL);
            return -1;
        }
    }

    // czy pętla zakończyła się sukcesem
    if (read_count < 0) {
        TEMP_FAILURE_RETRY(close(fd_from));
        TEMP_FAILURE_RETRY(close(fd_to));
        sigprocmask(SIG_SETMASK, &oldmask, NULL);
        return -1;
    }

    // ? kopiowanie atrybutów pliku
    if (fchown(fd_to, stat_buf.st_uid, stat_buf.st_gid) < 0 && errno != EPERM && errno != EOPNOTSUPP) {
        TEMP_FAILURE_RETRY(close(fd_from));
        TEMP_FAILURE_RETRY(close(fd_to));
        sigprocmask(SIG_SETMASK, &oldmask, NULL);
        return -1;
    }

    struct timespec times[2];
    times[0] = stat_buf.st_atim; // Czas ostatniego dostępu
    times[1] = stat_buf.st_mtim; // Czas ostatniej modyfikacji

    if (futimens(fd_to, times) < 0) {
        TEMP_FAILURE_RETRY(close(fd_from));
        TEMP_FAILURE_RETRY(close(fd_to));
        sigprocmask(SIG_SETMASK, &oldmask, NULL);
        return -1;
    }

    // ? zwolnienie zasobów
    if (TEMP_FAILURE_RETRY(close(fd_from)) < 0 || TEMP_FAILURE_RETRY(close(fd_to)) < 0) {
        sigprocmask(SIG_SETMASK, &oldmask, NULL);
        return -1;
    }

    // odblokowanie sygnałów
    if (sigprocmask(SIG_SETMASK, &oldmask, NULL) == -1) {
        return -1;
    }

    if (sig_stop == 1) {
        errno = EINTR;
        bulk_write(STDERR_FILENO, "[INFO] Interrupted while copying files. Not all files may be copied.");
        return -1;
    }

    return 0;
}

int remove_callback(const char *fpath, const struct stat *sb, int typeflag, struct FTW *ftwbuf) {
    // ! Chwilowe zablokowanie sygnałów — sekcja krytyczna
    sigset_t mask, oldmask;
    sigemptyset(&mask);

    sigaddset(&mask, SIGINT);
    sigaddset(&mask, SIGTERM);

    if (sigprocmask(SIG_BLOCK, &mask, &oldmask) == -1) {
        return -1;
    }

    int res = remove(fpath);
    if (res != 0) {
        if (errno == ENOENT) {
            return 0;
        }
        if (errno == EACCES) {
            // nie jest to błąd krytyczny, więc można pominąć
            return 0;
        }

        return -1;
    }

    // odblokowanie sygnałów
    if (sigprocmask(SIG_SETMASK, &oldmask, NULL) == -1) {
        return -1;
    }

    if (sig_stop == 1) {
        errno = EINTR;
        bulk_write(STDERR_FILENO, "[INFO] Interrupted while removing files. Not all files might be deleted.");
        return -1;
    }

    return 0;
}
