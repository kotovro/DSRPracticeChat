#include <stdio.h>
#include "user_storage.h"
#include "utils.h"


#define USER_STORAGE_PATH "/tmp/server/" 
#define USER_STORAGE_FILE "/tmp/server/users.lst" 

int user_count = 0;
user_data users[MAX_CLIENTS + 1];

static void commit() {
    FILE *fptr;

    fptr = fopen(USER_STORAGE_FILE, "w");

    if (fptr == NULL) {
        printf("Ошибка открытия файла.\n");
        return;
    }

    for (int i = 0; i < user_count; i++) {
        fprintf(fptr, "Username:%s\nIs global chat disabled:%d\nIs moderator:%d\nIs global chat banned:%d\n",  
            users[i].username, users[i].is_global_chat_disabled, users[i].is_moderator, users[i].is_global_chat_banned);
    } 

    fclose(fptr);
}

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

int init_user_storage() {
    user_count = 1;
    strcpy(users[0].username, "Незарегистрированный пользователь");

    mkdir_p(USER_STORAGE_PATH, 0755);
    FILE *fptr;

    fptr = fopen(USER_STORAGE_FILE, "r");

    if (fptr == NULL) {
        printf("Ошибка открытия файла.\n");
        return 0;
    }

    char line[MAX_MSG_LEN + 6];

    while (fgets(line, sizeof(line), fptr) != NULL ) {
        line[strcspn(line, "\n")] = '\0';
        char copy[MAX_MSG_LEN + 6];
        strcpy(copy, line);
        char *token = strtok(copy, ":"); 
        if (strcmp(token, "Username") == 0) {
            token = strtok(NULL, ":");
            strcpy(users[user_count].username, token);
        } else 
        if (strcmp(token, "Is global chat disabled") == 0) {
            token = strtok(NULL, ":");
            users[user_count].is_global_chat_disabled =  atoi(token);
        } else 
        if (strcmp(token, "Is moderator") == 0) {
            token = strtok(NULL, ":");
            users[user_count].is_moderator = atoi(token);
        } else
        if (strcmp(token, "Is global chat banned") == 0) {
            token = strtok(NULL, ":");
            users[user_count].is_global_chat_banned = atoi(token);
            user_count++;
        } 
    }
    return user_count - 1;
}

int add_user(char *username) {
    if (user_count > MAX_CLIENTS) {
        return -1;
    }
    strcpy(users[user_count].username, username);
    user_count++;
    commit();

    return user_count - 1;
}

void change_user_mute(int user_id, bool is_muted) {
    commit();
    users[user_id].is_global_chat_disabled = is_muted;
}