#ifndef CLIENT_LOGIN_H
#define CLIENT_LOGIN_H

#include <stdbool.h>
#include <stddef.h>
#include "../common/commons.h"
#include "../common/settings.h"
#include "user_storage.h"


int login(message_format *login_info);

#endif // CLIENT_LOGIN_H

