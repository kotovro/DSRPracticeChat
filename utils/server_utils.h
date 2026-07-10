#ifndef SERVER_UTILS_H
#define SERVER_UTILS_H

#include "commons.h"

message_format create_server_message(message_type type, const char *destination);


#endif // SERVER_UTILS_H