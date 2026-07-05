#include "client_login.h"

#include <stdio.h>
#include <stddef.h>
#include <string.h>

#define LOGIN_COMMAND "login "
#define LOGIN_COMMAND_LEN strlen(LOGIN_COMMAND)

// TODO: Превреить уникальность имени пользователя, добавить проверку на запрещенные символы и т.д.
// TODO: Добавить проверку забанен или нет пользователь 
bool validate_username(const char *username) {
    if (username == NULL || strlen(username) == 0 || strlen(username) >= MAX_NAME_LEN) {
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

    printf("Клиент %s сменил имя на: %s\n", msg->text + LOGIN_COMMAND_LEN, data->username);

    memcpy(data->username, msg->text + LOGIN_COMMAND_LEN, name_len);
    
    data->username[name_len] = '\0';
    printf("Обработка логина: %s\n", data->username);
    data->is_logged_in = true; // Отметим, что пользователь вошел

    // Здесь можно добавить логику для обработки логина клиента
    // Например, проверка имени пользователя, сохранение в базе данных и т.д.
    return true;
}