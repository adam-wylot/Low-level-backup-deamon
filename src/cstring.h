#ifndef CSTRING_H
#define CSTRING_H

#define INITBUFSIZE 2

#include <stdio.h>
#include <sys/types.h>

// * ===== STRUKTURA =====
typedef struct {
	char *buf;
	ssize_t length;
	ssize_t capacity;
} *cstring_t;


// * Deklaracje funkcji:
cstring_t cstr_init(void);

void cstr_free(cstring_t *cstr);

int cstr_append(cstring_t cstr, char ch);

char *cstr_getstr(cstring_t cstr);

void cstr_reset(cstring_t cstr);

#endif /// CSTRING_H
