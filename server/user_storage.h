#ifndef USER_STORAGE_H
#define USER_STORAGE_H

#include "commons.h"

int find_user_by_name(char *name);
int init_user_storage();
user_data *get_user_by_id(int id);
int add_user(char *username);
void change_user_mute(int user_id, bool is_muted);
void change_user_global_ban(int user_id, bool is_banned);

#endif // USER_STORAGE_H