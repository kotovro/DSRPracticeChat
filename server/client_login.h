#ifndef CLIENT_LOGIN_H
#define CLIENT_LOGIN_H

#include <stdbool.h>
#include <stddef.h>
#include "../common/commons.h"
#include "../common/settings.h"
#include "user_storage.h"


bool login(message_format *login_info, client_data *data);

#endif // CLIENT_LOGIN_H

