#ifndef MESSAGE_STORAGE_H
#define MESSAGE_STORAGE_H

#include "commons.h"

int save_message(/* char *file_path */ message_format *message);
message_format *get_message_by_id(char *guid);
void edit_message_text(message_format *msg_to_edit, char *new_text);
void delete_message(message_format *msg_to_delete);
int init_message_storage();

#endif // MESSAGE_STORAGE_H