#ifndef CONSOLEIO_H
#define CONSOLEIO_H

#include "command.h"

int read_command(command_t cmd);

ssize_t bulk_write(int fd, char *buf);

ssize_t bulk_write2(int fd, char *buf, size_t count);

ssize_t bulk_read(int fd, char *buf, size_t count);

#endif /// CONSOLEIO_H
