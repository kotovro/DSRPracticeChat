#ifndef GROUP_STORAGE_H
#define GROUP_STORAGE_H

#include "commons.h"

bool add_user_to_group(int user_id, int group_id);
int find_group_by_name(const char *group_name);
int create_group(const char *group_name);
bool is_user_in_group(int user_id, int group_id);
bool is_user_banned_in_group(int user_id, int group_id);
group_data *get_group_by_id(int group_id);
int iterate_groups(int current_id);
int init_group_storage();

#endif //GROUP_STORAGE_H