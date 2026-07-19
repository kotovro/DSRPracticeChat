#ifndef FILE_STORAGE_H
#define FILE_STORAGE_H

#include <stdio.h>
#include <stdbool.h>
#include "settings.h"

typedef struct {
    char localname[GUID_LEN + 1];
    char sharedname[GUID_LEN + 1];
    char clientname[MAX_FILENAME_LEN];
    char source[MAX_NAME_LEN];
    char destination[MAX_NAME_LEN];
} file_name_mapping;

file_name_mapping *get_file_by_localname(char *name);
file_name_mapping *get_file_by_sharedname(char *name);
int open_shared_file(char *shared_filename, FILE **file_pointer);
void add_file_mapping(char *system_filename, char *client_filename, char *source, const char *destination);
void update_file_mapping(char *system_filename, char *shared_filename);
int create_file(char *system_filename, FILE **file_pointer);
bool file_mapping_exsts(char *name);
int init_file_storage();

#endif //FILE_STORAGE_H