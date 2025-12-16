#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE

#include "cstring.h"
#include <stdio.h>
#include <stdlib.h>

// * funkcje pomocnicze:
int cstr_grow(cstring_t cstr) {
	char *new_buf = (char *) realloc(cstr->buf, 2 * cstr->capacity * sizeof(char));
	if (new_buf == NULL) {
		return -1;
	}

	cstr->buf = new_buf;
	cstr->capacity *= 2;

	return 0;
}


// * ===== Definicje funkcji =====
cstring_t cstr_init(void) {
	cstring_t elem = (cstring_t) malloc(sizeof *elem);
	if (elem == NULL) {
		return NULL;
	}

	elem->buf = (char *) malloc(INITBUFSIZE * sizeof(char));
	if (elem->buf == NULL) {
		free(elem);
		return NULL;
	}

	elem->length = 0;
	elem->capacity = INITBUFSIZE;
	elem->buf[0] = '\0';

	return elem;
}

void cstr_free(cstring_t *cstr) {
	if (cstr == NULL || *cstr == NULL) {
		return;
	}

	if ((*cstr)->buf != NULL) {
		free((*cstr)->buf);
	}
	free(*cstr);
	*cstr = NULL;
}

int cstr_append(cstring_t cstr, char ch) {
	if (cstr->length + 1 >= cstr->capacity) {
		if (cstr_grow(cstr) == -1) {
			return -1;
		}
	}

	cstr->buf[cstr->length++] = ch;
	cstr->buf[cstr->length] = '\0';

	return 0;
}

char *cstr_getstr(cstring_t cstr) {
	char *str = (char *) malloc((cstr->length + 1) * sizeof(char));
	if (str == 0) {
		return NULL;
	}

	snprintf(str, cstr->length + 1, "%s", cstr->buf);
	return str;
}

void cstr_reset(cstring_t cstr) {
	cstr->length = 0;
	cstr->buf[0] = '\n';
}
