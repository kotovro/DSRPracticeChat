#include "group_storage.h"

#define GROUP_STORAGE_PATH "/tmp/server/groups.lst" 

static int groups_count = 0;
static group_data groups[MAX_GROUPS];

int find_group_by_name(const char *group_name) {
    for (uint16_t i = 0; i < groups_count; i++) {
        if (strcmp(group_name, groups[i].group_name) == 0) {
            return i;
        }
    }
    return -1;
}

int create_group(const char *group_name) {
    if (find_group_by_name(group_name) >= 0 || groups_count >= MAX_GROUPS) {
        return -1;
    }
    
    strcpy(groups[groups_count].group_name, group_name);
    groups_count++;
    return groups_count - 1;
}

bool is_user_in_group(int user_id, int group_id) {
    for (int i = 0; i < groups[group_id].members_count; i++) {
        if (groups[group_id].members_list[i] == user_id) return true;
    }
    return false;
}

bool is_user_banned_in_group(int user_id, int group_id) {
    for (int i = 0; i < groups[group_id].banned_users_count; i++) {
        if (groups[group_id].banned_users_list[i] == user_id) return true;
    }
    return false;
}

bool add_user_to_group(int user_id, int group_id) {
    if (group_id >= groups_count) {
        return false;
    }
    group_data *group = groups + group_id;
    if (is_user_in_group(user_id, group_id) || group->members_count >= MAX_CLIENTS) {
        return false;
    }
    group->members_list[group->members_count] = user_id;
    group->members_count++;
    return true;
}

group_data *get_group_by_id(int group_id) {
    return groups + group_id;
}

int iterate_groups(int current_id) {
    current_id++;
    if (current_id < 0 || current_id >= groups_count) {
        return -1;
    }
    return current_id; 
}

void init_group_storage() {

}