#ifndef USER_STORAGE_H
#define USER_STORAGE_H

#include "commons.h"

int find_user_by_name(char *name);
void init_user_storage();
user_data *get_user_by_id(int id);
int add_user(char *username);
void change_user_mute(int user_id, bool is_muted);

#endif // USER_STORAGE_H