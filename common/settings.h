#ifndef SETTINGS_H
#define SETTINGS_H

#define MAX_CLIENTS 100
#define MAX_MSG_LEN 1024
#define MAX_NAME_LEN 128
#define MAX_FILENAME_LEN 260
#define MAX_FILES_STORED 10
#define MAX_MESSAGES_STORED 256 
#define MAX_GROUP_NAME_LEN MAX_NAME_LEN
#define GUID_LEN 36
#define MAX_FILE_SIZE ((long long)(10 * 1024 * 1024))
#define MSG_TEMPLATE "yyyy-MM-dd hh:mm:ss {} [] для []: \n"
#define LOG_MESSAGE_LEN strlen(MSG_TEMPLATE) + GUID_LEN + MAX_NAME_LEN + MAX_GROUP_NAME_LEN + MAX_MSG_LEN + GUID_LEN + 1
#define MAX_GROUPS 16
#define MAX_QUEUE 2
#define SERVER_PORT 8080
#define SERVER_IP "127.0.0.1"
#define SERVER_NAME "Сервер"
#define WELCOME_MESSAGE "Добро пожаловать в чат, %s!"
#define JOIN_MESSAGE "Пользователь %s присоединился к чату."
#define LOGIN_FAIL_MESSAGE "Ошибка: Вы не вошли в систему. Используйте команду /login <имя_пользователя>."
#define GLOBAL_CHAT_NAME "Общий чат"


#endif // SETTINGS_H