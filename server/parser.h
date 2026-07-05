#ifndef PARSER_H
#define PARSER_H

#include "commons.h"

message_format try_execute_command(const char *command, const char *destination, client_data *client);

#endif