#include "client_login.h"

#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include "group_storage.h"

#define LOGIN_COMMAND "login "
#define LOGIN_COMMAND_LEN strlen(LOGIN_COMMAND)

// TODO: Превреить уникальность имени пользователя, добавить проверку на запрещенные символы и т.д.
// TODO: Добавить проверку забанен или нет пользователь 
bool validate_username(const char *username) {
    if (username == NULL || strlen(username) == 0 || strlen(username) >= MAX_NAME_LEN || strcmp(username, SERVER_NAME) == 0 || strcmp(username, GLOBAL_CHAT_NAME) == 0) {
        return false;
    }
    // Дополнительные проверки на допустимые символы можно добавить здесь
    return true;
}

bool login(message_format *msg, client_data *data) {
    if (msg == NULL 
        || data == NULL 
        || msg->type != COMMAND
        || strlen(msg->text) <= LOGIN_COMMAND_LEN
        || strncmp(msg->text, LOGIN_COMMAND, LOGIN_COMMAND_LEN) != 0) {
        return false;
    }
    size_t name_len = strlen(msg->text) - LOGIN_COMMAND_LEN;
    if (name_len >= MAX_NAME_LEN) name_len = MAX_NAME_LEN - 1;
    
    if (!validate_username(msg->text + LOGIN_COMMAND_LEN)) {
        return false; // Имя пользователя не прошло валидацию
    }

    int user_id = find_user_by_name(msg->text + LOGIN_COMMAND_LEN);
    
    if (user_id <= 0) {
        int group_id = find_group_by_name(msg->text + LOGIN_COMMAND_LEN);
        if (group_id >= 0) {
            return false;
        }
        user_id = add_user(msg->text + LOGIN_COMMAND_LEN);
        if (user_id <= 0) {
            return false;
        }
    } else {
        return false;
    }

    data->user_id = user_id;
    printf("Клиент сменил имя на: %s, id %d, %d \n", msg->text + LOGIN_COMMAND_LEN, data->user_id, user_id);

    // Здесь можно добавить логику для обработки логина клиента
    // Например, проверка имени пользователя, сохранение в базе данных и т.д.
    return true;
}