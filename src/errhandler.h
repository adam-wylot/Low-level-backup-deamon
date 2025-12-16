#ifndef ERRHANDLER_H
#define ERRHANDLER_H

#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <signal.h>

#define ERR(source) \
	fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), \
	perror(source), \
	kill(0, SIGKILL), \
	exit(EXIT_FAILURE)
#endif /// ERRHANDLER_H
