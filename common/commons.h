#ifndef COMMON_H
#define COMMON_H

#include <stdbool.h>
#include <libwebsockets.h>
#include "../common/settings.h"

// Структура для сообщения в очереди
typedef struct out_msg {
    char data[LWS_PRE + MAX_MSG_LEN];
    size_t len;
} out_msg;

// Структура для хранения данных каждого подключенного клиента
typedef struct client_data {
    struct lws *wsi;
    bool is_logged_in; // Флаг для проверки, вошел ли пользователь в чат
    char username[MAX_NAME_LEN];

    // Очередь исходящих сообщений для этого конкретного клиента
    out_msg queue[MAX_QUEUE];
    int queue_count;
} client_data;

#endif // COMMON_H