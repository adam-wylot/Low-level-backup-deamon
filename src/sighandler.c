#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE

#include "sighandler.h"
#include "errhandler.h"
#include <signal.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>


// * Zmienne globalne do obsługi sygnałów
volatile sig_atomic_t sig_stop = 0;
volatile sig_atomic_t sig_child = 0;


// * Funkcje
void set_handler(void (*f)(int), int signum) {
	struct sigaction act = {0};
	act.sa_handler = f;

	if (sigaction(signum, &act, NULL) == -1) {
		// ! Błąd krytyczny
		ERR("sigaction");
	}
}

void sig_stop_handler(int sig) { sig_stop = 1; }

void sig_child_handler(int sig) { sig_child = 1; }

void ignore_all_signals() {
	sigset_t sigset;
	sigemptyset(&sigset);

	// przygotowanie 'handlera'
	struct sigaction act = {0};
	act.sa_handler = SIG_IGN;

	// iterowanie po wszystkich numerach sygnałóœ
	for (int i = 1; i < NSIG; i++) {
		// pominięcie ważnych sygnałów
		if (i == SIGKILL || i == SIGSTOP || i == SIGSEGV || i == SIGFPE || i == SIGILL || i == SIGBUS || i == SIGCHLD) {
			continue;
		}

		if (sigaddset(&sigset, i) == -1) {
			continue;
		}

		if (sigaction(i, &act, NULL) == -1) {
			// ! Błąd krytyczny
			ERR("sigaction");
		}
	}
}
