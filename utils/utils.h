#ifndef UTILS_H
#define UTILS_H

#include "commons.h"
void generate_uuid(char *uuid_str);

void message_to_json(const message_format *msg, char *json_str, size_t max_len);
void message_from_json(const char *json_str, message_format *msg);

int extract_bracket_word(const char *str, char *out, size_t out_size);
char* ltrim(char *s);
char* rtrim(char *s);

#endif // UTILS_H