#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE

#include "filehandler.h"
#include "hashmap.h"
#include "sighandler.h"
#include <errno.h>
#include <ftw.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/inotify.h>
#include <poll.h>

#include "consoleio.h"

// * Stałe inotify
#define MAXFD 20
#define MAX_EVENTS 1024
#define EVENT_SIZE (sizeof(struct inotify_event))
#define EVENT_BUF_LEN (MAX_EVENTS * (EVENT_SIZE + 16))


// * Zmienne globalne
const char *DEST_PATH_G = NULL;
int INOTIFY_FD_G = -1;
hashmap_t wd_map_G = NULL;
// ! Zmienne globalne, ponieważ nie ma innego sposobu na przekazanie tych rzeczy do funkcji walker do nftw :(


// * Deklaracje funkcji
int init_backup_walker(const char *, const struct stat *, int, struct FTW *);

int clean_source_walker(const char *, const struct stat *, int, struct FTW *);

int restore_walker(const char *, const struct stat *, int, struct FTW *);

int init_watch_only_walker(const char *, const struct stat *, int, struct FTW *);

int watch_loop(int fd, hashmap_t map, const char *src_root, const char *dest_root);

int perform_restore(char *src, char *dest);


// * Definicje funkcji
int make_backup(char *src, char *dest, int watch_only) {
	int status = 0;

	if (!watch_only) {
		// ? Sprawdzanie poprawności katalou source
		switch (dir_exists(src)) {
			case -2: return 2;
			case -3: return 3;
			case 0: return 4;
			case 1: break;
			default:
			case -1: return 1;
		}

		// ? Sprawdzanie, czy <dest> nie leży w środku <source>
		if ((status = is_subdirectory(src, dest)) != 0) {
			if (status == 1) {
				return 6;
			}
		}

		// ? Sprawdzanie poprawności katalou dest
		switch (dir_exists(dest)) {
			case -2: return 2;
			case -3: return 3;
			case 0:
				// ? katalog nie istnieje — tworzę go
				if ((status = mkdir_recursive(dest, 0755)) != 0) {
					return -status;
				}
				break;

			case 1:
				// ? katalog istnieje
				if ((status = is_dir_empty(dest)) != 1) {
					if (status == 0) {
						return 5;
					}

					return -status;
				}
				break;

			default:
			case -1: return 1;
		}
	}


	// ? Inicjalizacja inotify
	int inotify_fd = inotify_init();
	if (inotify_fd < 0) {
		return 1;
	}

	hashmap_t wd_map = hashmap_init();
	if (wd_map == NULL) {
		TEMP_FAILURE_RETRY(close(inotify_fd));
		return 1;
	}

	INOTIFY_FD_G = inotify_fd;
	wd_map_G = wd_map;

	// ? kopiowanie całej struktury plików rekursywnie
	/// Wybranie walkera:
	// — jeśli watch_only == 1 (po restore), chcemy tylko ustawić inotify na nowych plikach (nowe inodes)
	// — jeśli watch_only == 0 (bazowo), używamy pełnej kopii z inotify
	int (*walker_func)(const char *, const struct stat *, int, struct FTW *) = watch_only
																				   ? init_watch_only_walker
																				   : init_backup_walker;

	DEST_PATH_G = dest;
	if (nftw(src, walker_func, MAXFD, FTW_PHYS) == -1) {
		int saved_errno = errno;
		TEMP_FAILURE_RETRY(close(inotify_fd));
		if (errno == 0) {
			errno = saved_errno;
		}

		hashmap_free(&wd_map);
		return -1;
	}
	DEST_PATH_G = NULL;

	// ? Monitorowanie pliku
	if (watch_loop(inotify_fd, wd_map, src, dest) == -1) {
		int saved_errno = errno;
		TEMP_FAILURE_RETRY(close(inotify_fd));
		if (errno == 0) {
			errno = saved_errno;
		}

		hashmap_free(&wd_map);
		INOTIFY_FD_G = -1;
		wd_map_G = NULL;
		return 1;
	}

	// ? Zwalnianie zasobów
	TEMP_FAILURE_RETRY(close(inotify_fd));
	hashmap_free(&wd_map);
	INOTIFY_FD_G = -1;
	wd_map_G = NULL;

	// nie sprawdzam sig_stop, bo i tak dziecko zakończy pracę

	return 0;
}

int perform_restore(char *src, char *dest) {
	// ? Usunięcie z SRC plików, których nie ma w DEST
	DEST_PATH_G = dest;

	// przeszukiwanie w sposób DFS, by najpierw usunąć zawartość
	nftw(src, clean_source_walker, MAXFD, FTW_PHYS | FTW_DEPTH);

	// ? Kopiowanie dest do src
	DEST_PATH_G = src;

	if (nftw(dest, restore_walker, MAXFD, FTW_PHYS) == -1) {
		// ! Błąd krytyczny przywracania kopii
		return -1;
	}

	return 0;
}

int watch_loop(int fd, hashmap_t map, const char *src_root, const char *dest_root) {
	char buffer[EVENT_BUF_LEN];

	// ? Inicjalizacja poll
	struct pollfd pfd;
	pfd.fd = fd;
	pfd.events = POLLIN;

	while (sig_stop == 0) {
		// ? Czy katalog bazowy nadal istnieje
		struct stat st_root;
		if (stat(src_root, &st_root) != 0) {
			// katalog bazowy nie istnieje — koniec oglądania
			return 0;
		}

		// ? pobieranie informacji o eventach
		int status = poll(&pfd, 1, 500);
		if (status == -1) {
			if (errno == EINTR) {
				continue;
			}

			return -1;
		}
		if (status == 0) {
			// minął timeout
			continue;
		}

		if (pfd.revents & POLLIN) {
			ssize_t len = read(fd, buffer, EVENT_BUF_LEN); // bez bulk_read, bo może być przerwane w każdym momencie
			if (len < 0) {
				if (errno == EINTR) {
					continue;
				}

				return -1;
			}

			int i = 0;
			while (i < len) {
				struct inotify_event *event = (struct inotify_event *) &buffer[i];

				if (event->mask & IN_Q_OVERFLOW) {
					if (bulk_write(STDERR_FILENO, "[WARNING] Inotify queue overflow! Rescanning source...\n") == -1) {
						return -1;
					}
					// uruchamiamy pełny skan naprawczy
					DEST_PATH_G = dest_root;
					if (nftw(src_root, init_backup_walker, MAXFD, FTW_PHYS) == -1) {
						bulk_write(STDERR_FILENO, "[CRITICAL] Failed to resync after overflow!\n");
						return -1;
					}
					DEST_PATH_G = NULL;

					// przechodzimy do następnego eventu
					i += (int) (event->len + EVENT_SIZE);
					continue;
				}

				char *src_dir = hashmap_get_path(map, &event->wd, sizeof(int));

				if (event->len > 0 && src_dir != NULL) {
					// budowanie pełnej ścieżki src
					size_t src_len = strlen(src_dir) + strlen(event->name) + 2;
					char *src_path = (char *) malloc(src_len);
					if (src_path == NULL) {
						// ! Błąd krytyczny
						return -1;
					}
					snprintf(src_path, src_len, "%s/%s", src_dir, event->name);

					// budowanie pełnej ścieżki dest
					size_t dest_len = 0;
					char *dest_path = NULL;

					const char *scrap_of_path = src_path + strlen(src_root);
					dest_len = strlen(dest_root) + strlen(scrap_of_path) + 1;
					dest_path = (char *) malloc(dest_len);
					if (dest_path == NULL) {
						// ! Błąd krytyczny
						free(src_path);
						return -1;
					}
					snprintf(dest_path, dest_len, "%s%s", dest_root, scrap_of_path);


					// * ============= OPERACJE NA PLIKACH ===============
					// ? (1) Nowy katalog
					if ((event->mask & IN_CREATE) && (event->mask & IN_ISDIR)) {
						if (mkdir(dest_path, 0755) == -1 && errno != EEXIST) {
							// ! Błąd krytyczny
							free(dest_path);
							free(src_path);
							return -1;
						}

						// dodanie watch'a na nowy katalog
						int new_wd = inotify_add_watch(fd, src_path,
													   IN_CREATE | IN_DELETE | IN_MODIFY | IN_MOVED_FROM |
													   IN_MOVED_TO | IN_CLOSE_WRITE | IN_DELETE_SELF |
													   IN_MOVE_SELF);
						if (new_wd >= 0) {
							if (hashmap_add(map, src_path, &new_wd, sizeof(new_wd)) != 0) {
								// ! Błąd krytyczny
								free(dest_path);
								free(src_path);
								return -1;
							}
						}
					}
					// ? (2) Modyfikacja LUB Utworzenie pliku
					else if ((event->mask & IN_CLOSE_WRITE) ||
							 ((event->mask & IN_CREATE) && !(event->mask & IN_ISDIR))) {
						struct stat st;
						if (lstat(src_path, &st) == 0) {
							if (S_ISLNK(st.st_mode)) {
								// to jest symlink — trzeba go odtworzyć
								ssize_t sympath_len = st.st_size;
								char *sympath = (char *) malloc((sympath_len + 1) * sizeof(char));
								if (sympath == NULL) {
									// ! Błąd krytyczny
									free(dest_path);
									free(src_path);
									return -1;
								}

								if (readlink(src_path, sympath, sympath_len) != -1) {
									sympath[sympath_len] = '\0';

									// Relatywizacja (jak w walkerze)
									if (sympath[0] == '/') {
										if (is_subdirectory(src_root, sympath)) {
											size_t new_len = strlen(sympath) - strlen(src_root) + strlen(dest_root) + 1;
											char *new_sympath = malloc(new_len);
											if (new_sympath) {
												snprintf(new_sympath, new_len, "%s%s", dest_root,
														 sympath + strlen(src_root));
												free(sympath);
												sympath = new_sympath;
											}
										}
									}

									// usunięcie starego linku w dest i stworzenie nowego
									unlink(dest_path);
									if (symlink(sympath, dest_path) == 0) {
										lchown(dest_path, st.st_uid, st.st_gid);
									}
								}
								free(sympath);
							} else if (!S_ISDIR(st.st_mode)) {
								// to zwykły plik
								if (copy_file(src_path, dest_path) == -1) {
									// ! Błąd krytyczny
									free(dest_path);
									free(src_path);
									return -1;
								}
							}
						}
					}

					// ? (3) Usunięcie
					else if (event->mask & IN_DELETE || event->mask & IN_MOVED_FROM) {
						struct stat st_d;

						if (lstat(dest_path, &st_d) == 0) {
							if (S_ISDIR(st_d.st_mode)) {
								// dla katalogu
								if (nftw(dest_path, remove_callback, MAXFD, FTW_PHYS | FTW_DEPTH) == -1) {
									// ! Błąd krytyczny
									free(dest_path);
									free(src_path);
									return -1;
								}

								hashmap_remove(map, src_path); // usunięcie tego wpisu z hashmap'y
							} else {
								// dla plików
								unlink(dest_path);
							}
						}
					}

					//?  (4) Przeniesienie
					else if (event->mask & IN_MOVED_TO) {
						if (event->mask & IN_ISDIR) {
							// dla katalogu
							DEST_PATH_G = dest_path;
							if (nftw(src_path, init_backup_walker, MAXFD, FTW_PHYS) == -1) {
								// ! Błąd krytyczny
								free(dest_path);
								free(src_path);
								return -1;
							}
							DEST_PATH_G = NULL;
						} else {
							// dla plików
							copy_file(src_path, dest_path);
						}
					}

					// ? Zwolnienie zasobów
					free(src_path);
					free(dest_path);
				}

				i += (int) (event->len + EVENT_SIZE);
			}
		}

		if (pfd.revents & (POLLERR | POLLHUP)) {
			// ! Błąd krytyczny deskryptora
			return -1;
		}
	}

	return 0;
}


// * ============================= WALKERZY: ==================================

int init_backup_walker(const char *name, const struct stat *s, int type, struct FTW *f) {
	int status = 0;

	// ? Znalezienie ścieżki katalogu startowego
	char *root_path = (char *) malloc((strlen(name) + 2) * sizeof(char));
	if (root_path == NULL) {
		// ! Błąd krytyczny
		return -1;
	}

	sprintf(root_path, "%s", name);

	for (int i = 0; i < f->level; ++i) {
		char *last_slash = strrchr(root_path, '/');
		if (last_slash != NULL) {
			// Odcięcie
			*last_slash = '\0';
		}
	}

	// Przypadek specjalny — src to katalog główny
	if (strlen(root_path) == 0) {
		sprintf(root_path, "/");
	}

	// ? budowanie ścieżki docelowej
	const char *scrap_of_name = name + strlen(root_path);

	size_t final_path_size = strlen(scrap_of_name) + strlen(DEST_PATH_G) + 1;
	char *final_path = (char *) malloc(final_path_size * sizeof(char));
	if (final_path == NULL) {
		// ! Błąd pamięci
		free(root_path);
		return -1;
	}

	// ! EDGE CASE
	if (strcmp(root_path, "/") == 0) {
		// dla roota musimy ręcznie dodać slash, bo scrap go nie ma
		snprintf(final_path, final_path_size, "%s/%s", DEST_PATH_G, scrap_of_name);
	} else {
		// dla reszty
		snprintf(final_path, final_path_size, "%s%s", DEST_PATH_G, scrap_of_name);
	}

	// ? sprawdzenie typu i wykonanie odpowiedniej operacji
	switch (type) {
		case FTW_DNR: // katalog, ale EACCES
		case FTW_D:
			// katalog
			if (mkdir(final_path, 0755) == -1 && errno != EEXIST && errno != EACCES) {
				// ! Błąd lub przerwanie (errno)
				status = -1;
			}

			if (type == FTW_DNR) {
				// Jeżeli EACCES to po co go śledzić
				break;
			}

			// ? Dodanie inotify na ten katalog
			int wd = inotify_add_watch(INOTIFY_FD_G, name,
									   IN_CREATE | IN_DELETE | IN_MODIFY | IN_MOVED_FROM | IN_MOVED_TO | IN_CLOSE_WRITE
									   | IN_DELETE_SELF);

			if (wd == -1) {
				status = -1;
				break;
			}
			if (hashmap_add(wd_map_G, (char *) name, &wd, sizeof(wd)) == -1) {
				// ! Błąd krytyczny
				status = -1;
				break;
			}
			break;

		default:
		case FTW_F:
			// plik lub coś innego — kopiowanie do katalogu docelowego
			if ((status = copy_file(name, final_path)) < 0) {
				break;
			}
			break;

		case FTW_SL:
			// symlink
			struct stat sb;
			if (lstat(name, &sb) == -1) {
				if (errno == EACCES) {
					break;
				}

				status = -1;
				break;
			}
			ssize_t sympath_len = sb.st_size; // długość ścieżki w symlink'u to wielkość pliku (symlink'a)

			char *sympath = (char *) malloc((sympath_len + 2) * sizeof(char));
			if (sympath == NULL) {
				status = -1;
				break;
			}

			// Odczytanie symlinka
			if (readlink(name, sympath, sympath_len) == -1) {
				if (errno == EACCES) {
					free(sympath);
					break;
				}

				free(sympath);
				status = -1;
				break;
			}
			sympath[sympath_len] = '\0'; // readlink nie dodaje znaku '\0'

			// ? Sprawdzenie gdzie wskazuje symlink
			if (sympath[0] == '/') {
				// ścieżka bezwzględna
				if (is_subdirectory(root_path, sympath)) {
					// symlink prowadzi do podkatalogu folderu źródłowego — zmiana sympath
					size_t new_sympath_size = strlen(sympath) - strlen(root_path) + strlen(DEST_PATH_G) + 1;
					char *new_sympath = (char *) malloc(new_sympath_size * sizeof(char));
					if (new_sympath == NULL) {
						free(sympath);
						status = -1;
						break;
					}

					snprintf(new_sympath, new_sympath_size, "%s%s", DEST_PATH_G, sympath + strlen(root_path));
					free(sympath);
					sympath = new_sympath;
				}
			}

			// ? Kopiowanie symlinka do miejsca docelowego
			if (symlink(sympath, final_path) != 0) {
				if (errno == EACCES) {
					free(sympath);
					break;
				}

				free(sympath);
				status = -1;
				break;
			}

			// ? Zwalnianie zasobów
			free(sympath);
			break;
	}

	// ? zwolnienie zasobów i zwrócenie wyniku
	free(root_path);
	free(final_path);
	return status;
}

int init_watch_only_walker(const char *name, const struct stat *s, int type, struct FTW *f) {
	// potrzebujemy tylko dodać watch na katalogi
	if (type == FTW_D || type == FTW_DNR) {
		if (INOTIFY_FD_G != -1 && wd_map_G != NULL) {
			int wd = inotify_add_watch(INOTIFY_FD_G, name,
									   IN_CREATE | IN_DELETE | IN_MODIFY | IN_MOVED_FROM | IN_MOVED_TO | IN_CLOSE_WRITE
									   | IN_DELETE_SELF);

			if (wd != -1) {
				if (hashmap_add(wd_map_G, (char *) name, &wd, sizeof(wd)) != 0) {
					return -1;
				}
			}
		}
	}
	return 0;
}

int clean_source_walker(const char *name, const struct stat *s, int type, struct FTW *f) {
	// ? Znalezienie ścieżki katalogu startowego
	char *root_path = (char *) malloc((strlen(name) + 2) * sizeof(char));
	if (root_path == NULL) {
		// ! Błąd krytyczny
		return -1;
	}
	sprintf(root_path, "%s", name);

	for (int i = 0; i < f->level; ++i) {
		char *last_slash = strrchr(root_path, '/');
		if (last_slash != NULL) {
			// Odcięcie
			*last_slash = '\0';
		}
	}

	// Przypadek specjalny — src to katalog główny
	if (strlen(root_path) == 0) {
		sprintf(root_path, "/");
	}

	// ? Budowanie ścieżki docelowej
	const char *scrap_of_name = name + strlen(root_path);
	size_t backup_path_len = strlen(DEST_PATH_G) + strlen(scrap_of_name) + 1;

	char *backup_path = (char *) malloc(backup_path_len);
	if (backup_path == NULL) {
		// ! Błąd krytyczny
		free(root_path);
		return -1;
	}

	// ! EDGE CASE
	if (strcmp(root_path, "/") == 0) {
		// dla roota musimy ręcznie dodać slash, bo scrap go nie ma
		snprintf(backup_path, backup_path_len, "%s/%s", DEST_PATH_G, scrap_of_name);
	} else {
		// dla reszty
		snprintf(backup_path, backup_path_len, "%s%s", DEST_PATH_G, scrap_of_name);
	}

	// ? Szukanie plików do usunięcia
	struct stat st_backup;
	if (lstat(backup_path, &st_backup) == -1 && errno == ENOENT) {
		// Plik nie istnieje w backupie -> usuwamy go ze źródła
		remove(name);
	}

	// ? Zwolnienie zasobów
	free(backup_path);
	free(root_path);
	return 0;
}

int restore_walker(const char *name, const struct stat *s, int type, struct FTW *f) {
	int status = 0;

	// ? Znalezienie ścieżki katalogu startowego
	char *root_path = (char *) malloc((strlen(name) + 2) * sizeof(char));
	if (root_path == NULL) {
		// ! Błąd krytyczny
		return -1;
	}

	sprintf(root_path, "%s", name);

	for (int i = 0; i < f->level; ++i) {
		char *last_slash = strrchr(root_path, '/');
		if (last_slash != NULL) {
			// Odcięcie
			*last_slash = '\0';
		}
	}

	// Przypadek specjalny — src to katalog główny
	if (strlen(root_path) == 0) {
		sprintf(root_path, "/");
	}

	// ? budowanie ścieżki docelowej
	const char *scrap_of_name = name + strlen(root_path);

	size_t final_path_size = strlen(scrap_of_name) + strlen(DEST_PATH_G) + 1;
	char *final_path = (char *) malloc(final_path_size * sizeof(char));
	if (final_path == NULL) {
		// ! Błąd pamięci
		free(root_path);
		return -1;
	}

	// ! EDGE CASE
	if (strcmp(root_path, "/") == 0) {
		// dla roota musimy ręcznie dodać slash, bo scrap go nie ma
		snprintf(final_path, final_path_size, "%s/%s", DEST_PATH_G, scrap_of_name);
	} else {
		// dla reszty
		snprintf(final_path, final_path_size, "%s%s", DEST_PATH_G, scrap_of_name);
	}

	// ? Kopiowanie plików
	struct stat st_source;
	int source_exists = (lstat(final_path, &st_source) == 0);
	int should_copy = 0;

	if (!source_exists) {
		should_copy = 1;
	} else {
		// Jeśli istnieje, sprawdzamy czas modyfikacji — kopiujemy, tylko jeśli backup jest nowszy
		if (s->st_mtime != st_source.st_mtime) {
			should_copy = 1;
		}
	}

	if (should_copy) {
		switch (type) {
			case FTW_DNR:
			case FTW_D:
				if (mkdir(final_path, 0755) == -1 && errno != EEXIST && errno != EACCES) {
					status = -1;
				}
				break;

			case FTW_SL:
				// symlink
				struct stat sb;
				if (lstat(name, &sb) == -1) {
					if (errno == EACCES) {
						break;
					}

					status = -1;
					break;
				}
				ssize_t sympath_len = sb.st_size; // długość ściezki w symlinku to wielkość pliku (symlinka)

				char *sympath = (char *) malloc((sympath_len + 2) * sizeof(char));
				if (sympath == NULL) {
					status = -1;
					break;
				}

				// Odczytanie symlinka
				if (readlink(name, sympath, sympath_len) == -1) {
					if (errno == EACCES) {
						free(sympath);
						break;
					}

					free(sympath);
					status = -1;
					break;
				}
				sympath[sympath_len] = '\0'; // readlink nie dodaje znaku '\0'

				// ? Sprawdzenie gdzie wskazuje symlink
				if (sympath[0] == '/') {
					// ścieżka bezwzględna
					if (is_subdirectory(root_path, sympath)) {
						// symlink prowadzi do podkatalogu folderu źródłowego — zmiana sympath
						size_t new_sympath_size = strlen(sympath) - strlen(root_path) + strlen(DEST_PATH_G) + 1;
						char *new_sympath = (char *) malloc(new_sympath_size * sizeof(char));
						if (new_sympath == NULL) {
							free(sympath);
							status = -1;
							break;
						}

						snprintf(new_sympath, new_sympath_size, "%s%s", DEST_PATH_G, sympath + strlen(root_path));
						free(sympath);
						sympath = new_sympath;
					}
				}

				// ? Kopiowanie symlinka do miejsca docelowego
				if (symlink(sympath, final_path) != 0) {
					if (errno == EACCES) {
						free(sympath);
						break;
					}

					free(sympath);
					status = -1;
					break;
				}

				// ? Zwalnianie zasobów
				free(sympath);
				break;

			default:
			case FTW_F:
				if (copy_file(name, final_path) == -1) {
					status = -1;
				}
				break;
		}
	}

	// ? Zwolnienie zasobów
	free(root_path);
	free(final_path);
	return status;
}
