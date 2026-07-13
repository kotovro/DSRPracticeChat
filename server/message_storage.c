#include "message_storage.h"
#include "user_storage.h"
#include "utils.h"

#define MESSAGE_STORAGE_PATH "/tmp/server/" 
#define MESSAGE_STORAGE_FILE "/tmp/server/messages.lst"

int messages_count = 0;  
message_format messages[MAX_MESSAGES_STORED];

static void commit() {
    FILE *fptr;

    fptr = fopen(MESSAGE_STORAGE_FILE, "w");

    if (fptr == NULL) {
        printf("Ошибка открытия файла.\n");
        return;
    }

    int last_index = messages_count > MAX_MESSAGES_STORED ? MAX_MESSAGES_STORED : messages_count;
    for (int i = 0; i < last_index; i++) {
        fprintf(fptr, "Type:%d\nDestination:%s\nSource:%s\nGUID:%s\nTime created:%ld\nTime modified:%ld\nTime deleted:%ld\nText:%s\n",  
            messages[i].type, messages[i].destination, messages[i].source, messages[i].message_guid, messages[i].time_created, messages[i].time_modified, messages[i].time_deleted, messages[i].text);
    } 

    fclose(fptr);
}

int save_message(/* char* file_name, */ message_format *message) {
    int current_index = messages_count % MAX_MESSAGES_STORED;
    generate_uuid(message->message_guid);
    memcpy(messages + current_index, message, sizeof(message_format));
    messages_count++;
    commit();
    return messages_count - 1;
}

message_format *get_message_by_id(char *guid) {
    int last_index = messages_count > MAX_MESSAGES_STORED ? MAX_MESSAGES_STORED : messages_count;
    for (int i = 0; i < last_index; i++) {
        if (strcmp(messages[i].message_guid, guid) == 0) {
            return messages + i;
        }
    } 
    return NULL;
}

void edit_message_text(message_format *msg_to_edit, char *new_text) {
    strcpy(msg_to_edit->text, new_text);
    msg_to_edit->time_modified = time(NULL);
}

int init_message_storage() {
    mkdir_p(MESSAGE_STORAGE_PATH, 0755);
    FILE *fptr;

    fptr = fopen(MESSAGE_STORAGE_FILE, "r");

    if (fptr == NULL) {
        printf("Ошибка открытия файла.\n");
        return 0;
    }

    char line[MAX_MSG_LEN + 6];
    
    while (fgets(line, sizeof(line), fptr) != NULL ) {
        line[strcspn(line, "\n")] = '\0';
        printf("%s, %d \n", line, messages_count);
        char copy[MAX_MSG_LEN + 6];
        strcpy(copy, line);
        char *token = strtok(copy, ":"); 
        if (strcmp(token, "Type") == 0) {
            token = strtok(NULL, ":");
            messages[messages_count % MAX_MESSAGES_STORED].type = atoi(token);
        } else 
        if (strcmp(token, "Destination") == 0) {
            token = strtok(NULL, ":");
            strcpy(messages[messages_count % MAX_MESSAGES_STORED].destination, token);
        } else 
        if (strcmp(token, "Source") == 0) {
            token = strtok(NULL, ":");
            strcpy(messages[messages_count % MAX_MESSAGES_STORED].source, token);
        } else
        if (strcmp(token, "GUID") == 0) {
            token = strtok(NULL, ":");
            strcpy(messages[messages_count % MAX_MESSAGES_STORED].message_guid, token);
        } else 
        if (strcmp(token, "Time created") == 0) {
            token = strtok(NULL, ":");
            messages[messages_count % MAX_MESSAGES_STORED].time_created = atoi(token);
        } else 
        if (strcmp(token, "Time modified") == 0) {
            token = strtok(NULL, ":");
            messages[messages_count % MAX_MESSAGES_STORED].time_modified = atoi(token);
        } else
        if (strcmp(token, "Time deleted") == 0) {
            token = strtok(NULL, ":");
            messages[messages_count % MAX_MESSAGES_STORED].time_deleted = atoi(token);
        } else 
        if (strcmp(token, "Text") == 0) {
            token = strtok(NULL, ":");
            strcpy(messages[messages_count % MAX_MESSAGES_STORED].text, token);
            messages_count++;
        }
    }
    return messages_count;
}