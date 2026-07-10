#include "user_storage.h"

int user_count = 0;
user_data users[MAX_CLIENTS + 1];

int find_user_by_name(char *name) {
    for (uint16_t i = 0; i < user_count; i++) {
        if (strcmp(name, users[i].username) == 0) {
            return i;
        }
    }
    return -1;
}

user_data *get_user_by_id(int id) {
    return users + id;
}

void init_user_storage() {
    user_count = 1;
    strcpy(users[0].username, "Незарегистрированный пользователь");
}

int add_user(char *username) {
    if (user_count > MAX_CLIENTS) {
        return -1;
    }
    strcpy(users[user_count].username, username);
    user_count++;
    
    return user_count - 1;
}

void change_user_mute(int user_id, bool is_muted) {
    users[user_id].is_global_chat_disabled = is_muted;
}