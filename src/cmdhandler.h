#ifndef CMDHANDLER_H
#define CMDHANDLER_H

#include "command.h"
#include "hashmapcomplex.h"

int proc_command(command_t cmd, hashmap_cmplx_t pathtable);

#endif /// CMDHANDLER_H
