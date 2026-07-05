#include "server_utils.h"
#include "utils.h"

message_format create_server_message(message_type type, const char* destination) {
    message_format msg = {0};
    msg.type = type;
    snprintf(msg.source, sizeof(msg.source), "%s", SERVER_NAME);
    snprintf(msg.destination, sizeof(msg.destination), "%s", destination);
    generate_uuid(msg.message_guid);
    msg.time_created = time(NULL);
    
    return msg;
}

