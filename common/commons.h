#ifndef COMMON_H
#define COMMON_H

#include <time.h>
#include <stdbool.h>
#include <libwebsockets.h>
#include "../common/settings.h"

typedef enum {
    TEXT,
    COMMAND,
    LOGIN_SUCCESS
} message_type;

typedef struct message_format {
    message_type type; // либо просто сообщение с текстом, либо команда с аргументами 
    char destination[MAX_GROUP_NAME_LEN];      // "область применения" "global", "group:<name>"
    char source[MAX_NAME_LEN];
    char message_guid[37]; // UUID string (36 characters + null terminator)
    char text[MAX_MSG_LEN];
    time_t time_created;
    time_t time_modified;
    time_t time_deleted;
} message_format;

// Структура для хранения данных каждого подключенного клиента
typedef struct client_data {
    struct lws *wsi;
    bool is_logged_in; // Флаг для проверки, вошел ли пользователь в чат
    char username[MAX_NAME_LEN];
    bool is_global_chat_disabled; // Флаг для проверки, отключен ли общий чат

    // Очередь исходящих сообщений для этого конкретного клиента
    message_format queue[MAX_QUEUE];
    int queue_count;
} client_data;



#endif // COMMON_H