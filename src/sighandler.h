#ifndef SIGHANDLER_H
#define SIGHANDLER_H

#include <signal.h>


// * Zmienne globalne
extern volatile sig_atomic_t sig_stop;
extern volatile sig_atomic_t sig_child;


// * Deklaracje funkcji
void ignore_all_signals();

void set_handler(void (*f)(int), int signum);

void sig_stop_handler(int sig);

void sig_child_handler(int sig);

#endif /// SIGHANDLER_H
