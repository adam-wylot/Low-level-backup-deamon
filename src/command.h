#ifndef COMMAND_H
#define COMMAND_H

#include "cstring.h"

typedef enum {
	CMD_INVALID = -1,
	CMD_NULL = 0,
	CMD_ADD,
	CMD_END,
	CMD_LIST,
	CMD_RESTORE,
	CMD_EXIT
} cmd_type_t;

typedef struct {
	cmd_type_t type;
	char **params;
	ssize_t pms_count;
} *command_t;

// * DEKLARACJE FUNKCJI
command_t cmd_init(void);

void cmd_free(command_t *cmd);

void cmd_set(command_t cmd, const char *str);

int cmd_add_param(command_t cmd, char *str);

void cmd_reset(command_t cmd);

int cmd_is_valid(command_t cmd);

#endif /// COMMAND_H
