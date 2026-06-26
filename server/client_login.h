#ifndef CLIENT_LOGIN_H
#define CLIENT_LOGIN_H

#include <stdbool.h>
#include <stddef.h>
#include "../common/commons.h"
#include "../common/settings.h"

bool login(char *login_info, size_t len, client_data *data);

#endif

