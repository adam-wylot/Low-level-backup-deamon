#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE

#include "cmdhandler.h"
#include "command.h"
#include "consoleio.h"
#include "sighandler.h"
#include "hashmap.h"
#include "hashmapcomplex.h"
#include "backuper.h"
#include <sys/wait.h>
#include <sys/types.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>

#define LIST_WRITE(msg) \
do { \
	if (bulk_write(STDOUT_FILENO, (msg)) == -1) { \
		return -1; \
	}\
} while(0);

int pathtable_print(hashmap_cmplx_t pathtable) {
	if (pathtable == NULL) {
		LIST_WRITE("*no backup is being made*\n");
		return 0;
	}

	size_t printed_flag = 0;

	for (int i = 0; i < HASH_TABLE_SIZE; ++i) {
		hm_c_node_t srcNode = pathtable->nodes[i];
		while (srcNode != NULL) {
			LIST_WRITE(srcNode->path);
			LIST_WRITE(":\n");
			printed_flag = 1;

			hashmap_t dests = srcNode->dests;
			if (dests != NULL) {
				for (int j = 0; j < HASH_TABLE_SIZE; ++j) {
					hm_node_t destNode = dests->nodes[j];
					while (destNode != NULL) {
						LIST_WRITE("\t-> ");
						LIST_WRITE(destNode->path);
						LIST_WRITE("\n");
						destNode = destNode->next;
					}
				}
			}

			srcNode = srcNode->next;
		}
	}

	if (!printed_flag) {
		LIST_WRITE("*no backup is being made*\n");
	}

	return 0;
}

int do_list(hashmap_cmplx_t pathtable) {
	// ? 64 miejsca w wierszu
	// nagłówek
	LIST_WRITE("================================================================\n");
	LIST_WRITE("                            PATTERN:\n");
	LIST_WRITE(">>>>>>>>>>>>>>>>>>>>>----------------------<<<<<<<<<<<<<<<<<<<<<\n");
	LIST_WRITE("<source path>:\n");
	LIST_WRITE("\t-> <backup destination 1>\n\t-> <backup destination 2>\n\t...\n");
	LIST_WRITE("================================================================\n");
	LIST_WRITE("                          BACKUP LIST:\n");
	LIST_WRITE(">>>>>>>>>>>>>>>>>>>>>----------------------<<<<<<<<<<<<<<<<<<<<<\n");

	// Backup list
	if (pathtable_print(pathtable) == -1) {
		return -1;
	}

	// zamknięcie
	LIST_WRITE("================================================================\n");
	return 0;
}

int do_add(command_t cmd, hashmap_cmplx_t pathtable) {
	pid_t pid = 0;
	int res = 0;

	char *src = cmd->params[0];
	char *dest = NULL;

	for (int i = 1; i < cmd->pms_count; ++i) {
		dest = cmd->params[i];

		// ? Sprawdzenie czy dest się nie powtarza
		if (hashmap_cmplx_contains(pathtable, src, dest)) {
			size_t msg_len = strlen("[ERROR] Pair x -> x already exists!\n") + strlen(src) + strlen(dest) + 1;
			char *msg = (char *) malloc(msg_len * sizeof(char));
			if (msg == NULL) {
				// ! Błąd krytyczny
				return -1;
			}

			snprintf(msg, msg_len, "[ERROR] Backup %s -> %s already exists!\n", src, dest);
			if (bulk_write(STDERR_FILENO, msg) == -1) {
				// ! Błąd krytyczny
				free(msg);
				return -1;
			}

			free(msg);
			continue;
		}

		if (hashmap_cmplx_is_dest_used(pathtable, dest)) {
			size_t msg_len = strlen("[ERROR] Destination directory x is already used by another backup! Check list.\n")
							 + strlen(dest) + 1;
			char *msg = (char *) malloc(msg_len * sizeof(char));
			if (msg == NULL) {
				// ! Błąd krytyczny
				return -1;
			}

			snprintf(msg, msg_len, "[ERROR] Destination directory %s is already used by another backup! Check list.\n",
					 dest);
			if (bulk_write(STDERR_FILENO, msg) == -1) {
				// ! Błąd krytyczny
				free(msg);
				return -1;
			}

			free(msg);
			continue;
		}

		// ? Tworzenie nowych procesów
		switch (pid = fork()) {
			case -1:
				// ! Błąd krytyczny fork()
				return -1;
			case 0:
				// praca dziecka
				res = make_backup(src, dest, 0);
				if (res == 1 && errno == EINTR && sig_stop == 1) {
					res = 0;
				}

				_exit(res);

			default:
				// praca rodzica — próba dodania nowego backupu
				if (hashmap_cmplx_add(pathtable, src, dest, &pid, sizeof(pid_t)) != 0) {
					// ! Błąd krytyczny
					return -1;
				}
				break;
		}
	}

	return 0;
}

int do_restore(command_t cmd, hashmap_cmplx_t pathtable) {
	char *src = cmd->params[0];
	char *dest = cmd->params[1];
	pid_t pid = 0;

	// ? Czy taki backup jest dostępny
	void *val = hashmap_cmplx_getval(pathtable, src, dest);
	if (val == NULL) {
		size_t msg_len = strlen("[ERROR] No backup for x -> x\n") + strlen(src) + strlen(dest) + 1;
		char *msg = (char *) malloc(msg_len);
		if (msg == NULL) {
			// ! Błąd krytyczny
			return -1;
		}

		snprintf(msg, msg_len, "[ERROR] No backup for %s -> %s\n", src, dest);
		if (bulk_write(STDERR_FILENO, msg) == -1) {
			// ! Błąd krytyczny
			free(msg);
			return -1;
		}

		free(msg);
		return 0;
	}
	pid = *(pid_t *) val;

	// ? Zatrzymanie aktualnego backup'u
	// jeżeli taki proces nie istnieje to nic straconego
	if (pid > 0) {
		if (kill(pid, SIGTERM) == -1 && errno != ESRCH) {
			// ! Błąd krytyczny
			return -1;
		}

		// zebranie procesu
		if (waitpid(pid, NULL, 0) < 0 && errno != ECHILD) {
			// ! Błąd krytyczny
			return -1;
		}
	}

	// ? Usunięcie starego wpisu z mapy par
	hashmap_cmplx_remove(pathtable, src, dest); // jeżeli para nie istnieje to nie jest to krytyczny błąd, więc pomijam

	// ? Wykonanie backup'u
	if (perform_restore(src, dest) != 0) {
		// ! Błąd bardzo krytyczny
		bulk_write(STDERR_FILENO, "[ERROR] Attention! Failed to restore backup!\n");
		return -1;
	}

	// ? Odtworzenie procesu
	pid_t new_pid = 0;
	switch (new_pid = fork()) {
		case -1:
			// ! Błąd krytyczny
			return -1;

		case 0:
			// praca dziecka
			int res = make_backup(src, dest, 1); // watch_only ustawia tylko inotify, nie kopiuje plików

			if (res == -1 && errno == EINTR) {
				res = 0;
			}
			_exit(res);

		default: break;
	}

	// praca rodzica
	if (hashmap_cmplx_add(pathtable, src, dest, &new_pid, sizeof(pid_t)) != 0) {
		// ! Nie udało się dodać — błąd krytyczny lub para już istnieje, a nie powinna
		return -1;
	}

	if (bulk_write(STDOUT_FILENO, "[INFO] Restoring completed.\n") == -1) {
		return -1;
	}

	return 0;
}

int do_end(command_t cmd, hashmap_cmplx_t pathtable) {
	char *src = cmd->params[0];
	char *dest = cmd->params[1];

	pid_t pid = *(pid_t *) hashmap_cmplx_getval(pathtable, src, dest);
	if (pid < 0) {
		// nie znaleziono takiej pary src-dest
		size_t msg_len = strlen("[ERROR] No backup for x -> x\n") + strlen(src) + strlen(dest) + 1;
		char *msg = (char *) malloc(msg_len);
		if (msg == NULL) {
			// ! Błąd krytyczny
			return -1;
		}

		snprintf(msg, msg_len, "[ERROR] No backup for %s -> %s\n", src, dest);
		if (bulk_write(STDERR_FILENO, msg) == -1) {
			// ! Błąd krytyczny
			free(msg);
			return -1;
		}

		free(msg);
		return 0;
	}

	if (kill(pid, SIGTERM) == -1) {
		// ! Błąd krytyczny
		return -1;
	}

	return 0;
}

int proc_command(command_t cmd, hashmap_cmplx_t pathtable) {
	switch (cmd->type) {
		case CMD_ADD: return do_add(cmd, pathtable);
		case CMD_LIST: return do_list(pathtable);
		case CMD_END: return do_end(cmd, pathtable);
		case CMD_RESTORE: return do_restore(cmd, pathtable);

		case CMD_EXIT:
			if (kill(0, SIGTERM) < 0) {
				return -1;
			}
			return 0;

		default: return 1;
	}
}
