#ifndef COMMON_H
#define COMMON_H

#include <time.h>
#include <stdbool.h>
#include <libwebsockets.h>
#include "settings.h"

typedef enum {
    TEXT,
    COMMAND,
    LOGIN_SUCCESS, 
    FILE_UPLOAD_ACK,
    FILE_UPLOAD_ANNOUNCE,
    FILE_DOWNLOAD_ACK
} message_type;

typedef enum {
    POST,
    GET
} http_request_type;

typedef struct message_format {
    message_type type; // либо просто сообщение с текстом, либо команда с аргументами 
    char destination[MAX_GROUP_NAME_LEN];      // "область применения" "global", "group:<name>"
    char source[MAX_NAME_LEN];
    char message_guid[GUID_LEN + 1]; // UUID string (36 characters + null terminator)
    char text[MAX_MSG_LEN];
    time_t time_created;
    time_t time_modified;
    time_t time_deleted;
} message_format;

typedef struct network_message {
    char network_prefix[LWS_PRE];
    message_format message;
} network_message;

// Структура для хранения данных каждого подключенного клиента
typedef struct client_data {
    struct lws *wsi;
    int user_id;
    char pending_filename[MAX_FILENAME_LEN];
    long long pending_filesize;
    char pending_token[GUID_LEN + 1];
    bool has_pending_upload;

    // Очередь исходящих сообщений для этого конкретного клиента
    int first_message;
    int queue_count;
    network_message queue[MAX_QUEUE];
} client_data;

typedef struct user_data {
    char username[MAX_NAME_LEN];
    bool is_global_chat_disabled; // Флаг для проверки, отключен ли общий чат
    bool is_moderator;
    bool is_global_chat_banned;
} user_data;

typedef struct group_data {
    int members_list[MAX_CLIENTS];
    int banned_users_list[MAX_CLIENTS];
    int banned_users_count;
    int members_count;
    char group_name[MAX_GROUP_NAME_LEN];
} group_data;

#endif // COMMON_H