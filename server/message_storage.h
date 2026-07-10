#ifndef MESSAGE_STORAGE_H
#define MESSAGE_STORAGE_H

#include "commons.h"

int save_message(/* char *file_path */ message_format *message);
message_format *get_message_by_id(char *guid);

#endif // MESSAGE_STORAGE_H