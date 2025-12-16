#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE

#include "sighandler.h"
#include "consoleio.h"
#include "errhandler.h"
#include "command.h"
#include "cmdhandler.h"
#include "hashmapcomplex.h"
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>


// * Deklaracje funkcji:
void usage(char *name);

void command_list();

void check_children(hashmap_cmplx_t pathtable);


// * ===== MAIN =====
int main(int argc, char *argv[]) {
	if (argc > 1) {
		// podano zbędne argumenty
		usage(argv[0]);
	}

	// ? Przygotowanie programu
	ignore_all_signals();
	set_handler(sig_stop_handler, SIGINT);
	set_handler(sig_stop_handler, SIGTERM);
	set_handler(sig_child_handler, SIGCHLD);

	command_t cmd = cmd_init();
	if (cmd == NULL) {
		// ! Błąd krytyczny
		ERR("malloc");
	}

	hashmap_cmplx_t pathtable = hashmap_cmplx_init();
	if (pathtable == NULL) {
		// ! Błąd krytyczny
		cmd_free(&cmd);
		ERR("hashmap_cmplx_init");
	}

	// --- START PROGRAMU ---
	command_list();

	while (sig_stop == 0) {
		check_children(pathtable);

		errno = 0;
		if (read_command(cmd) == -1) {
			if (errno == EINTR) {
				// EINTR
				if (sig_stop == 1) {
					// celowe przerwanie
					break;
				}

				if (sig_child == 1) {
					check_children(pathtable);
					sig_child = 0;
					continue; // Próbuj czytać dalej
				}
			}

			// ! Błąd krytyczny
			hashmap_cmplx_free(&pathtable);
			cmd_free(&cmd);
			ERR("read_command");
		}

		// validowanie
		if (cmd->type == CMD_NULL) {
			// TODO: koniec np. przy wczytywaniu poleceń z pliku
			continue;
		}

		if (cmd_is_valid(cmd) == 0) {
			if (cmd->type == CMD_INVALID) {
				if (bulk_write(STDERR_FILENO, "Unknown command.\n") == -1) {
					if (errno == EINTR && sig_stop == 1) {
						// jeżeli był SIGCHLD to zostanie obsłużony przy nastepnej iteracji pętli
						break;
					}

					// ! Błąd krytyczny
					hashmap_cmplx_free(&pathtable);
					cmd_free(&cmd);
					ERR("bulk_write");
				}

				command_list();
			} else {
				if (bulk_write(STDERR_FILENO, "Invalid arguments.\n") == -1) {
					if (errno == EINTR && sig_stop == 1) {
						// jeżeli był SIGCHLD to zostanie obsłużony przy nastepnej iteracji pętli
						break;
					}

					// ! Błąd krytyczny
					hashmap_cmplx_free(&pathtable);
					cmd_free(&cmd);
					ERR("bulk_write");
				}
			}
			continue;
		}

		// * ===== Wykonywanie funkcji: =====
		int res = 0;
		while ((res = proc_command(cmd, pathtable)) != 0) {
			if (res == -1) {
				if (errno == EINTR) {
					if (sig_stop == 1) {
						break;
					}

					if (sig_child == 1) {
						check_children(pathtable);
						sig_child = 0;
						continue;
					}
				}

				// ! Błąd krytyczny
				hashmap_cmplx_free(&pathtable);
				cmd_free(&cmd);
				ERR("proc_command");
			}
			if (res == 1) {
				if (bulk_write(STDERR_FILENO, "Command execution failed. Command list:\n") == -
					1) {
					if (errno == EINTR && sig_stop == 1) {
						break;
					}

					// ! Błąd krytyczny
					hashmap_cmplx_free(&pathtable);
					cmd_free(&cmd);
					ERR("bulk_write");
				}

				command_list();
			}
		}
	}

	// * --- Kończenie programu ---
	bulk_write(STDOUT_FILENO, "\nShutting down...\n");

	// ? zatrzymanie pracy innych procesów
	set_handler(SIG_IGN, SIGINT);
	set_handler(SIG_IGN, SIGTERM);
	set_handler(SIG_IGN, SIGCHLD);

	kill(0, SIGTERM); // jedyny błąd, jaki może wystąpić to EPERM

	// ? czyszczenie zasobów
	hashmap_cmplx_free(&pathtable);
	cmd_free(&cmd);

	while (wait(NULL) > 0) {
	}

	return EXIT_SUCCESS;
}

// * Definicje funkcji:
#define LOG(fd, str, ...) \
do { \
	char to_print[4096] = ""; \
	snprintf(to_print, 4096, str, ##__VA_ARGS__); \
	bulk_write(STDERR_FILENO, (to_print)); \
} while(0);

#define LOG_ERR(str, ...) LOG(STDERR_FILENO, str, ##__VA_ARGS__)

void check_children(hashmap_cmplx_t pathtable) {
	int wstatus;
	pid_t pid;

	while ((pid = waitpid(-1, &wstatus, WNOHANG)) > 0) {
		char *src_buf = NULL;
		char *dest_buf = NULL;
		int pair_found = 0;

		if (hashmap_cmplx_get_pair(pathtable, &pid, sizeof(pid), &src_buf, &dest_buf) ==
			-1) {
			pair_found = 1;
		}

		if (WIFEXITED(wstatus)) {
			int exit_status = WEXITSTATUS(wstatus);

			if (!pair_found) {
				exit_status = -1;
			}

			// ! Dziecko zakończyło się błędem — sprawdzam, czy krytycznym
			switch (exit_status) {
				case 0:
					LOG(STDOUT_FILENO,
						"\n[INFO] Child process %d finished work (source deleted or stopped).\n",
						pid)
					break;

				case 2:
					LOG_ERR(
						"[ERROR] Insufficient permissions to create backup at %s -> %s.\n",
						src_buf, dest_buf)
					break;
				case 3:
					LOG_ERR(
						"[ERROR] Source or destination path is not a directory (%s -> %s).\n",
						src_buf, dest_buf)
					break;
				case 4:
					LOG_ERR("[ERROR] Source directory %s does not exist.\n", src_buf);
					break;
				case 5:
					LOG_ERR("[ERROR] Destination directory %s is not empty.\n", dest_buf);
					break;

				case 6:
					if (strcmp(src_buf, dest_buf) == 0) {
						// dest to ten sam katalog co src
						LOG_ERR(
							"[ERROR] Destination directory %s is the same as source directory %s.\n",
							dest_buf, src_buf);
						break;
					}

					// dest leży w środku src
					LOG_ERR(
						"[ERROR] Destination directory %s is inside of source directory %s.\n",
						dest_buf, src_buf);
					break;

				default:
				case 1:
					LOG_ERR("[CRITICAL] Unknown error.\n");
					LOG_ERR("[CRITICAL] Child process %d exited with error code %d.\n", pid,
							exit_status);
					LOG_ERR("[CRITICAL] Terminating application...\n");
					ERR("CHILD FAULT");
			}
		} else if (WIFSIGNALED(wstatus)) {
			// ! Obsługa procesów zabitych sygnałem z zewnątrz snp SIGKILL
			int signum = WTERMSIG(wstatus);
			if (pair_found) {
				LOG_ERR("[CRITICAL] Child process %d (Backup: %s -> %s) killed by signal %d.\n",
						pid, src_buf, dest_buf, signum);
			} else {
				LOG_ERR("[CRITICAL] Unknown child process %d killed by signal %d.\n", pid, signum);
			}
		}

		// ? Usunięcie wpisu z tabeli
		if (pair_found) {
			hashmap_cmplx_remove(pathtable, src_buf, dest_buf);
		}
	}

	if (pid == -1 && errno != EINTR && errno != ECHILD) {
		ERR("waitpid");
	}
}

void usage(char *name) {
	LOG(STDOUT_FILENO, "USAGE: %s\n", name);
	LOG(STDOUT_FILENO,
		"Program runs in interactive mode. Use commands to control backups.\n");
	exit(EXIT_FAILURE);
}

void command_list() {
	LOG(STDOUT_FILENO, "AVAILABLE COMMANDS:\n");
	LOG(STDOUT_FILENO,
		"  -- add <src> <dest> ...  : Start backing up <src> to one or more <dest> locations.\n");
	LOG(STDOUT_FILENO,
		"  -- end <src> <dest>      : Stop backup process for specific pair.\n");
	LOG(STDOUT_FILENO,
		"  -- restore <src> <dest>  : Restore content from <dest> backup to <src>.\n");
	LOG(STDOUT_FILENO, "  -- list                  : List active backup tasks.\n");
	LOG(STDOUT_FILENO,
		"  -- exit                  : Stop all backups and exit program.\n\n");
}
