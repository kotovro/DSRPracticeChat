#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include "utils.h"
#include "commons.h"

void generate_uuid(char *uuid_str) {
    // Генерация UUID версии 4 (рандомный)
    const char *chars = "0123456789abcdef";
    for (int i = 0; i < 36; i++) {
        if (i == 8 || i == 13 || i == 18 || i == 23) {
            uuid_str[i] = '-';
        } else if (i == 14) {
            uuid_str[i] = '4'; // Версия UUID
        } else if (i == 19) {
            uuid_str[i] = chars[(rand() % 4) + 8]; // Диапазон для варианта
        } else {
            uuid_str[i] = chars[rand() % 16];
        }
    }
    uuid_str[36] = '\0'; // Завершающий нулевой символ
}

void message_to_json(const message_format *msg, char *json_str, size_t max_len) {
    snprintf(json_str, max_len,
             "{\"type\":\"%s\",\"destination\":\"%s\",\"source\":\"%s\",\"message_guid\":\"%s\",\"text\":\"%s\"}",
             msg->type == TEXT ? "text" : "command",
             msg->destination,
             msg->source,
             msg->message_guid,
             msg->text);
}

void message_from_json(const char *json_str, message_format *msg) {
    // Простейший парсер JSON
    sscanf(json_str,
           "{\"type\":\"%[^\"]\",\"destination\":\"%[^\"]\",\"source\":\"%[^\"]\",\"message_guid\":\"%[^\"]\",\"text\":\"%[^\"]\"}",
           msg->type == TEXT ? "text" : "command",
           msg->destination,
           msg->source,
           msg->message_guid,
           msg->text);
}

int extract_bracket_word(const char *str, char *out, size_t out_size)
{
    if (!str || str[0] != '[') {
        return 0; // строка начинается не с открывающей скобки
    }

    const char *end = strchr(str, ']');
    if (!end) {
        out[0] = '\0'; // нет закрывающей скобки, возвращаем пустую строку
        return 0;
    }

    size_t len = end - (str + 1); // длина слова внутри скобок
    if (len == 0 || len + 1 > out_size) {
        return 1; // либо пустое значение, либо недостаточно места
    }

    memcpy(out, str + 1, len);
    out[len] = '\0';

    return 0; // успех
}


// returns pointer to trimmed string (may shift forward)
char* ltrim(char *s) {
    while (*s && isspace((unsigned char)*s)) {
        s++;
    }
    return s;
}

char* rtrim(char *s) {
    size_t len = strlen(s);
    while (len > 0 && isspace((unsigned char)s[len - 1])) {
        s[--len] = '\0';
    }
    return s;
}
