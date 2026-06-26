#include "client_login.h"

#include <stdio.h>
#include <stddef.h>
#include <string.h>

#define LOGIN_COMMAND "/login "
#define LOGIN_COMMAND_LEN strlen(LOGIN_COMMAND)

// TODO: Превреить уникальность имени пользователя, добавить проверку на запрещенные символы и т.д.
// TODO: Добавить проверку забанен или нет пользователь 
bool login(char *login_info, size_t len, client_data *data) {
    if (login_info == NULL 
        || data == NULL 
        || len <= LOGIN_COMMAND_LEN
        || strncmp(login_info, LOGIN_COMMAND, LOGIN_COMMAND_LEN) != 0) {
        return false;
    }
    

    size_t name_len = len - LOGIN_COMMAND_LEN;
    if (name_len >= MAX_NAME_LEN) name_len = MAX_NAME_LEN - 1;

    memcpy(data->username, login_info + LOGIN_COMMAND_LEN, name_len);
    
    data->username[name_len] = '\0';
    printf("Обработка логина: %s\n", data->username);
    data->is_logged_in = true; // Отметим, что пользователь вошел
    printf("[Система] Клиент сменил имя на: %s\n", data->username);

    // Здесь можно добавить логику для обработки логина клиента
    // Например, проверка имени пользователя, сохранение в базе данных и т.д.
    return true;

}