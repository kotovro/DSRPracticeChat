#include "group_storage.h"
#include "utils.h"

#define GROUP_STORAGE_PATH "/tmp/server/" 
#define GROUP_STORAGE_FILE "/tmp/server/groups.lst" 

int groups_count = 0;
group_data groups[MAX_GROUPS];

static void commit() {
    FILE *fptr;

    fptr = fopen(GROUP_STORAGE_FILE, "w");

    if (fptr == NULL) {
        printf("Ошибка открытия файла.\n");
        return;
    }

    for (int i = 0; i < groups_count; i++) {
        fprintf(fptr, "Banned user count:%d\nMembers count:%d\n",  
            groups[i].banned_users_count, groups[i].members_count);
        for (int j = 0; j < groups[i].members_count; j++) {
            fprintf(fptr, "member id:%d \n", groups[i].members_list[j]);
        }
        for (int j = 0; j < groups[i].banned_users_count; j++) {
            fprintf(fptr, "banned id:%d \n", groups[i].members_list[j]);
        }
        fprintf(fptr, "Group name:%s\n", groups[i].group_name);
    } 

    fclose(fptr);
}

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
    commit();
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
    commit();
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

int init_group_storage() {
    mkdir_p(GROUP_STORAGE_PATH, 0755);
    FILE *fptr;

    fptr = fopen(GROUP_STORAGE_FILE, "r");

    if (fptr == NULL) {
        printf("Ошибка открытия файла.\n");
        return 0;
    }

    char line[MAX_MSG_LEN + 6];
    int current_memeber_index = 0;
    int current_banned_index = 0;
    while (fgets(line, sizeof(line), fptr) != NULL ) {
        line[strcspn(line, "\n")] = '\0';
        printf("%s, %d \n", line, groups_count);
        char copy[MAX_MSG_LEN + 6];
        strcpy(copy, line);
        char *token = strtok(copy, ":"); 
        if (strcmp(token, "Banned user count") == 0) {
            token = strtok(NULL, ":");
            groups[groups_count].banned_users_count = atoi(token);
        } else 
        if (strcmp(token, "Members count") == 0) {
            token = strtok(NULL, ":");
            groups[groups_count].members_count = atoi(token);
        } else 
        if (strcmp(token, "member id") == 0) {
            token = strtok(NULL, ":");
            groups[groups_count].members_list[current_memeber_index] = atoi(token);
            current_memeber_index++;
        } else 
        if (strcmp(token, "banned id") == 0) {
            token = strtok(NULL, ":");
            groups[groups_count].banned_users_list[current_banned_index] = atoi(token);
            current_banned_index++;
        } else 
        if (strcmp(token, "Group name") == 0) {
            token = strtok(NULL, ":");
            strcpy(groups[groups_count].group_name, token);
            groups_count++;
        }
    }
    return groups_count;
}