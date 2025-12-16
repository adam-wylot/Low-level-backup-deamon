#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE

#include "consoleio.h"
#include "command.h"
#include "cstring.h"
#include "sighandler.h"
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

void skip_line() {
	char last_char = 0;

	while (1) {
		if (read(STDIN_FILENO, &last_char, 1) <= 0 || last_char == '\n') {
			// EINTR lub EOF || koniec linii
			break;
		}
	}
}

int read_command(command_t cmd) {
	cstring_t cstr = NULL;
	ssize_t read_bytes = 0;
	size_t word_count = 0;
	int in_quotes_flag = 0;
	char prev_char = 0;
	char last_char = ' ';

	cmd_reset(cmd); // ? reset polecenia

	if ((cstr = cstr_init()) == NULL) {
		return -1;
	}

	while (1) {
		prev_char = last_char;
		read_bytes = bulk_read(STDIN_FILENO, &last_char, 1);
		if (read_bytes == 0) {
			// EOF
			if (cstr->length == 0 && word_count == 0) {
				// Jeśli nie wczytaliśmy jeszcze niczego, to oznacza koniec (przy np. wczytywaniu poleceń z pliku)
				cmd->type = CMD_EXIT;
				cstr_free(&cstr);
				return 0;
			}
			break;
		}
		if (read_bytes == -1) {
			cstr_free(&cstr);
			return -1;
		}
		if (last_char == '\n') {
			// koniec polecenia (\n lub EOF)
			break;
		}

		// ? obsługa znaków "
		if (last_char == '"') {
			if (prev_char == ' ' && in_quotes_flag == 0) {
				in_quotes_flag = 1;
				last_char = 0;
			}

			continue;
		}

		// ? obsługa spacji
		if (last_char == ' ') {
			if (in_quotes_flag == 0 || prev_char == '"') {
				// koniec "wyrazu"
				if (word_count == 0) {
					// to musi być polecenie
					cmd_set(cmd, cstr->buf);
				} else if (cstr->length > 0) {
					// ? dodanie parametru
					char *param = cstr->buf;

					// zrobienie z tego ścieżki absolutnej
					char *abs_path = NULL;
					char *cwd = NULL;

					if (param[0] == '/') {
						// Ścieżka absolutna
						abs_path = strdup(param);
					} else {
						// Ścieżka relatywna — sklej z cwd
						cwd = get_current_dir_name();
						if (cwd) {
							size_t len = strlen(cwd) + strlen(param) + 2;
							abs_path = malloc(len * sizeof(char));
							if (abs_path) {
								snprintf(abs_path, len, "%s/%s", cwd, param);
							}

							free(cwd);
						}
					}

					if (abs_path == NULL) {
						cstr_free(&cstr);
						return -1;
					}

					if (cmd_add_param(cmd, abs_path) < 0) {
						free(abs_path);
						cstr_free(&cstr);
						return -1;
					}

					// Usuwanie końcowego slasha (jeżeli to możliwe)
					size_t path_len = strlen(abs_path);
					if (path_len > 1 && abs_path[path_len - 1] == '/') {
						abs_path[path_len - 1] = '\0';
					}
				}

				in_quotes_flag = 0;
				++word_count;
				cstr_reset(cstr);
				continue;
			}
		} else if (prev_char == '"') {
			// dopisanie brakującego znaku "
			cstr_append(cstr, '"');
		}

		cstr_append(cstr, last_char);
	}

	// dodanie ostatniego parametru po znaku końca linii
	if (read_bytes == 1 && last_char == '\n' && cstr->length > 0) {
		if (word_count == 0) {
			// to musi być polecenie
			cmd_set(cmd, cstr->buf);
		} else {
			// dodanie parametru
			char *param = cstr->buf;

			// ? poprawienie ścieżki relatywna -> absolutna
			char *abs_path = NULL;
			char *cwd = NULL;

			if (param[0] == '/') {
				// Ścieżka absolutna
				abs_path = strdup(param);
			} else {
				// Ścieżka relatywna — sklej z cwd
				cwd = get_current_dir_name();
				if (cwd) {
					size_t len = strlen(cwd) + strlen(param) + 2;
					abs_path = malloc(len * sizeof(char));
					if (abs_path) {
						snprintf(abs_path, len, "%s/%s", cwd, param);
					}

					free(cwd);
				}
			}

			if (abs_path == NULL) {
				cstr_free(&cstr);
				return -1;
			}

			if (cmd_add_param(cmd, abs_path) < 0) {
				free(abs_path);
				cstr_free(&cstr);
				return -1;
			}

			// Usuwanie końcowego slasha (jeżeli to możliwe)
			size_t path_len = strlen(abs_path);
			if (path_len > 1 && abs_path[path_len - 1] == '/') {
				abs_path[path_len - 1] = '\0';
			}
		}
	}

	// zwalnianie zasobów
	cstr_free(&cstr);
	return 0;
}

ssize_t bulk_write(int fd, char *buf) {
	size_t count = strlen(buf);
	ssize_t c;
	ssize_t len = 0;

	do {
		c = TEMP_FAILURE_RETRY(write(fd, buf, count));
		if (c < 0) {
			return c;
		}

		buf += c;
		len += c;
		count -= c;
	} while (count > 0);

	if (sig_stop == 1) {
		errno = EINTR;
		return -1;
	}

	return len;
}

ssize_t bulk_write2(int fd, char *buf, size_t count) {
	ssize_t c;
	ssize_t len = 0;

	do {
		c = TEMP_FAILURE_RETRY(write(fd, buf, count));
		if (c < 0) {
			return c;
		}

		buf += c;
		len += c;
		count -= c;
	} while (count > 0);

	if (sig_stop == 1) {
		errno = EINTR;
		return -1;
	}

	return len;
}

ssize_t bulk_read(int fd, char *buf, size_t count) {
	ssize_t c;
	ssize_t len = 0;
	do {
		c = TEMP_FAILURE_RETRY(read(fd, buf, count));
		if (c < 0) {
			return c;
		}
		if (c == 0) {
			return len; // EOF
		}
		buf += c;
		len += c;
		count -= c;
	} while (count > 0);

	if (sig_stop == 1) {
		errno = EINTR;
		return -1;
	}

	return len;
}
