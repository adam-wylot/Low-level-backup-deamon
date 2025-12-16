#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE

#include "command.h"
#include <stdlib.h>
#include <string.h>

command_t cmd_init(void) {
	command_t cmd = (command_t) malloc(sizeof *cmd);
	if (cmd == NULL) {
		return NULL;
	}

	cmd->params = NULL;
	cmd->pms_count = 0;
	cmd->type = CMD_NULL;

	return cmd;
}

void cmd_free(command_t *cmd) {
	if (cmd == NULL || *cmd == NULL) {
		return;
	}

	for (int i = 0; i < (*cmd)->pms_count; ++i) {
		free((*cmd)->params[i]);
	}

	free((*cmd)->params);
	free(*cmd);

	*cmd = NULL;
}

void cmd_set(command_t cmd, const char *str) {
	if (str == NULL) {
		cmd->type = CMD_INVALID;
		return;
	}

	if (strcmp(str, "add") == 0) {
		cmd->type = CMD_ADD;
	} else if (strcmp(str, "end") == 0) {
		cmd->type = CMD_END;
	} else if (strcmp(str, "list") == 0) {
		cmd->type = CMD_LIST;
	} else if (strcmp(str, "restore") == 0) {
		cmd->type = CMD_RESTORE;
	} else if (strcmp(str, "exit") == 0) {
		cmd->type = CMD_EXIT;
	} else {
		cmd->type = CMD_INVALID;
	}
}

int cmd_add_param(command_t cmd, char *str) {
	char **new_params = (char **) realloc(cmd->params, (cmd->pms_count + 1) * sizeof(char *));
	if (new_params == NULL) {
		return -1;
	}

	cmd->params = new_params;
	cmd->params[cmd->pms_count++] = str;

	return 0;
}

void cmd_reset(command_t cmd) {
	for (int i = 0; i < cmd->pms_count; ++i) {
		free(cmd->params[i]);
	}

	free(cmd->params);
	cmd->params = NULL;
	cmd->pms_count = 0;
	cmd->type = CMD_NULL;
}

int cmd_is_valid(command_t cmd) {
	switch (cmd->type) {
		case CMD_ADD:
			if (cmd->pms_count < 2) {
				return 0;
			}
			break;

		case CMD_RESTORE:
		case CMD_END:
			if (cmd->pms_count != 2) {
				return 0;
			}
			break;

		case CMD_EXIT:
		case CMD_LIST:
			if (cmd->pms_count != 0) {
				return 0;
			}
			break;

		case CMD_INVALID:
			return 0;

		default: return 1;
	}

	return 1;
}
