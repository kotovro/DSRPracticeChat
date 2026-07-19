#include <string.h>
#include "file_storage.h"
#include "utils.h"

#define FILE_STORAGE_PATH "/tmp/server/" 
#define FILE_STORAGE_FILE "/tmp/server/files.lst" 

int files_count = 0;  
file_name_mapping files[MAX_FILES_STORED];

static void commit() {
    FILE *fptr;

    fptr = fopen(FILE_STORAGE_FILE, "w");

    if (fptr == NULL) {
        printf("Ошибка открытия файла.\n");
        return;
    }

    int last_index = files_count >= MAX_FILES_STORED ? MAX_FILES_STORED : files_count;
    for (int i = 0; i < last_index; i++) {
        fprintf(fptr, "Local file name:%s\nShared file name:%s\nClient file name:%s\nSource name:%s\nDestination:%s\n",  
            files[i].localname, files[i].sharedname, files[i].clientname, files[i].source, files[i].destination);
    } 

    fclose(fptr);
}

file_name_mapping *get_file_by_localname(char *name) {
    int last_index = files_count >= MAX_FILES_STORED ? MAX_FILES_STORED : files_count;
    for (int i = 0; i < last_index; i++) {
        if (strcmp(files[i].localname, name) == 0) {
            return files + i;        
        }
    }
    return NULL;
}

file_name_mapping *get_file_by_sharedname(char *name) {
    int last_index = files_count >= MAX_FILES_STORED ? MAX_FILES_STORED : files_count;
    for (int i = 0; i < last_index; i++) {
        if (strcmp(files[i].sharedname, name) == 0) {
            return files + i;        
        }
    }
    return NULL;
}

int open_shared_file(char *shared_filename, FILE **file_pointer) {
    file_name_mapping *shared_file = get_file_by_sharedname(shared_filename);
    if (shared_file == NULL) {
        return 1;
    }
    char full_filename[MAX_FILENAME_LEN];
    sprintf(full_filename, "%s%s", FILE_STORAGE_PATH, shared_file->localname);
    *file_pointer = fopen(full_filename, "rb");
    if (*file_pointer == NULL) {
        return -1;
    }
    return 0;
}

void get_filename_from_path(char *path, char *filename) {
    char *fname = strrchr(path, '/');
    if  (fname != NULL) {
        strcpy(filename, fname + 1);
    }
}

void add_file_mapping(char *systemfilename, char *clientfilename, char *source, const char *destination) {
    strcpy(files[files_count % MAX_FILES_STORED].localname, systemfilename);
    strcpy(files[files_count % MAX_FILES_STORED].source, source);
    strcpy(files[files_count % MAX_FILES_STORED].destination, destination);
    get_filename_from_path(clientfilename, files[files_count % MAX_FILES_STORED].clientname);
    files_count++;
    commit();
}

void update_file_mapping(char *systemfilename, char *sharedfilename) {
    int last_index = files_count >= MAX_FILES_STORED ? MAX_FILES_STORED : files_count;
    for (int i = 0; i < last_index; i++) {
        if (strcmp(files[i].localname, systemfilename) == 0) {
            strcpy(files[i].sharedname, sharedfilename);
            break;        
        }
    }
    commit();
}

int create_file(char *system_filename, FILE **file_pointer) {
    //Открываем файл на запись в бинарном режиме
    char full_filename[MAX_FILENAME_LEN];
    sprintf(full_filename, "%s%s", FILE_STORAGE_PATH, system_filename);
    *file_pointer = fopen(full_filename, "wb");
    if (*file_pointer == NULL) {
        return -1;
    }
    return 0;
}

bool file_mapping_exsts(char *name) {
    return get_file_by_localname(name) != NULL;
}

int init_file_storage() {
    mkdir_p(FILE_STORAGE_PATH, 0755);
    FILE *fptr;

    fptr = fopen(FILE_STORAGE_FILE, "r");

    if (fptr == NULL) {
        printf("Ошибка открытия файла с метаданными о файлах.\n");
        return 0;
    }

    char line[MAX_FILENAME_LEN + 6];
    
    while (fgets(line, sizeof(line), fptr) != NULL ) {
        line[strcspn(line, "\n")] = '\0';
        char copy[MAX_FILENAME_LEN + 6];
        strcpy(copy, line);
        char *token = strtok(copy, ":"); 
        if (strcmp(token, "Local file name") == 0) {
            token = strtok(NULL, ":");
            strcpy(files[files_count % MAX_FILES_STORED].localname, token);
        } else 
        if (strcmp(token, "Shared file name") == 0) {
            token = strtok(NULL, ":");
            strcpy(files[files_count % MAX_FILES_STORED].sharedname, token);
        } else 
        if (strcmp(token, "Client file name") == 0) {
            token = strtok(NULL, ":");
            strcpy(files[files_count % MAX_FILES_STORED].clientname, token);
        } else
        if (strcmp(token, "Source name") == 0) {
            token = strtok(NULL, ":");
            strcpy(files[files_count % MAX_FILES_STORED].source, token);
        } else 
        if (strcmp(token, "Destination") == 0) {
            token = strtok(NULL, ":");
            strcpy(files[files_count % MAX_FILES_STORED].destination, token);
            files_count++;
        }
    }
    return files_count;
}