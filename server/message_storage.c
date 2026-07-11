#include "message_storage.h"
#include "user_storage.h"
#include "utils.h"

int messages_count = 0;  
message_format messages[MAX_MESSAGES_STORED];

int save_message(/* char* file_name, */ message_format *message) {
    int current_index = messages_count % MAX_MESSAGES_STORED;
    generate_uuid(message->message_guid);
    memcpy(messages + current_index, message, sizeof(message_format));
    messages_count++;
    return messages_count - 1;
}

message_format *get_message_by_id(char *guid) {
    int last_index = messages_count > MAX_MESSAGES_STORED ? MAX_MESSAGES_STORED : messages_count;
    for (int i = 0; i < last_index; i++) {
        if (strcmp(messages[i].message_guid, guid) == 0) {
            return messages + i;
        }
    } 
    return NULL;
}

void edit_message_text(message_format *msg_to_edit, char *new_text) {
    strcpy(msg_to_edit->text, new_text);
    msg_to_edit->time_modified = time(NULL);
}

//get_user_by_name(msg.source) && get_group_by_name(msg.destination) ->  get_message_by_id(msg.guid)