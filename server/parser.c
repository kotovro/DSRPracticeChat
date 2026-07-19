#include "parser.h" 
#include "settings.h"
#include "utils.h"
#include "server_utils.h"
#include "user_storage.h"
#include "group_storage.h"
#include "message_storage.h"
#include "file_storage.h"

message_format try_execute_command(const char *command, const char *destination, client_data *client) {    
    message_format msg = create_server_message(TEXT, destination); 
    char copy_command[strlen(command) + 1];
    strcpy(copy_command, command);
    char *copy_command_end = copy_command + strlen(copy_command);

    char *token = strtok(copy_command, " ");
    if (strcmp(token, "mute") == 0) {
        if (strcmp(destination, GLOBAL_CHAT_NAME) == 0) {
            change_user_mute(client->user_id, true);
            snprintf(msg.text, sizeof(msg.text), "Теперь Вы не будете видеть сообщения, которые приходят в %s.", GLOBAL_CHAT_NAME);
            strcpy(msg.destination, get_user_by_id(client->user_id)->username);
        }
    } else if (strcmp(token, "unmute") == 0) {
        if (strcmp(destination, GLOBAL_CHAT_NAME) == 0) {
            change_user_mute(client->user_id, false);
            snprintf(msg.text, sizeof(msg.text), "Теперь Вы снова можете видеть сообщения, которые приходят в %s.", GLOBAL_CHAT_NAME);
            strcpy(msg.destination, get_user_by_id(client->user_id)->username);
        }
    } else if (strcmp(token, "add_group") == 0) {
        token = strtok(NULL, " ");
        if (token == NULL) {
            snprintf(msg.text, sizeof(msg.text), "Формат команды: /add_group <название группы>.");
            strcpy(msg.destination, get_user_by_id(client->user_id)->username);
            return msg;
        }
        if (find_user_by_name(token) >= 0) {
            snprintf(msg.text, sizeof(msg.text), "Группу %s создать невозможно.", token);
            strcpy(msg.destination, get_user_by_id(client->user_id)->username);
            return msg;
        }
        int res = create_group(token);
        if (res < 0) {
            snprintf(msg.text, sizeof(msg.text), "Группу %s создать невозможно.", token);
            strcpy(msg.destination, get_user_by_id(client->user_id)->username);
            return msg;
        } 
        add_user_to_group(client->user_id, res);
        snprintf(msg.text, sizeof(msg.text), "Группа %s успешно создана.", token);
        strcpy(msg.destination, token);
    } else if (strcmp(token, "add_user") == 0) {
        int group_id = find_group_by_name(destination); 
        if (group_id < 0) {
            snprintf(msg.text, sizeof(msg.text), "Группа %s не найдена.", token);
            strcpy(msg.destination, get_user_by_id(client->user_id)->username);
            return msg;
        }   
        if (!is_user_in_group(client->user_id, group_id)) {
            strcpy(msg.text, "Недостаточно прав для выполнения этой команды");
            strcpy(msg.destination, get_user_by_id(client->user_id)->username);
            return msg;
        } 
        token = strtok(NULL, " ");
        if (token == NULL) {
            snprintf(msg.text, sizeof(msg.text), "Формат команды: [<название группы>] /add_user <имя пользователя>.");
            strcpy(msg.destination, get_user_by_id(client->user_id)->username);
            return msg;
        }  
        int user_id = find_user_by_name(token);
        if (user_id <= 0) {
            snprintf(msg.text, sizeof(msg.text), "Пользователь %s не найден.", token);
            strcpy(msg.destination, get_user_by_id(client->user_id)->username);
            return msg;
        } 
        if (add_user_to_group(user_id, group_id)) {
            snprintf(msg.text, sizeof(msg.text), "Пользователь %s добавлен в группу.", token);
            return msg;
        } 
        snprintf(msg.text, sizeof(msg.text), "Пользователя %s невозможно добавить в группу.", token);
        strcpy(msg.destination, get_user_by_id(client->user_id)->username);
        return msg;
    } else if (strcmp(token, "edit") == 0) {
        token = strtok(NULL, " ");
        if (token == NULL) {
            snprintf(msg.text, sizeof(msg.text), "Формат команды: /edit <guid сообщения> <новый текст сообщения>.");
            strcpy(msg.destination, get_user_by_id(client->user_id)->username);
            return msg;
        }
        message_format *msg_to_edit = get_message_by_id(token);
        if (msg_to_edit == NULL || msg_to_edit->time_deleted > 0) {
            snprintf(msg.text, sizeof(msg.text), "Сообщение с guid %s невозомжно отредактировать.", token);
            strcpy(msg.destination, get_user_by_id(client->user_id)->username);
            return msg;
        }
        int editor_id = client->user_id;
        if (strcmp(msg_to_edit->source, get_user_by_id(editor_id)->username) != 0) {
            snprintf(msg.text, sizeof(msg.text), "У Вас недостаточно прав для редактирования данного сообщения.");
            strcpy(msg.destination, get_user_by_id(client->user_id)->username);
            return msg;
        }
        if (copy_command_end <= token + strlen(token)) {
            snprintf(msg.text, sizeof(msg.text), "Формат команды: /edit <guid сообщения> <новый текст сообщения>.");
            strcpy(msg.destination, get_user_by_id(client->user_id)->username);
            return msg;
        }
        char *new_text = token + strlen(token) + 1;
        edit_message_text(msg_to_edit, new_text);
        memcpy(&msg, msg_to_edit, sizeof(message_format));
        return msg;
    } else if (strcmp(token, "delete") == 0) {
        token = strtok(NULL, " ");
        if (token == NULL) {
            snprintf(msg.text, sizeof(msg.text), "Формат команды: /delete <guid сообщения>.");
            strcpy(msg.destination, get_user_by_id(client->user_id)->username);
            return msg;
        } 
        message_format *msg_to_delete = get_message_by_id(token);
        if (msg_to_delete == NULL) {
            snprintf(msg.text, sizeof(msg.text), "Сообщение с guid %s невозомжно удалить.", token);
            strcpy(msg.destination, get_user_by_id(client->user_id)->username);
            return msg;
        }
        int editor_id = client->user_id;
        if (!get_user_by_id(editor_id)->is_moderator) {
            strcpy(msg.text, "У Вас недостаточно прав для удаления данного сообщения.");
            strcpy(msg.destination, get_user_by_id(client->user_id)->username);
            return msg;
        }

        delete_message(msg_to_delete);
        memcpy(&msg, msg_to_delete, sizeof(message_format));
        strcpy(msg.text, "Сообщение удалено.");
        return msg;
    } else if (strcmp(token, "ban") == 0) {
        token = strtok(NULL, " ");
        if (token == NULL) {
            snprintf(msg.text, sizeof(msg.text), "Формат команды: /ban <имя пользователя>.");
            strcpy(msg.destination, get_user_by_id(client->user_id)->username);
            return msg;
        } 
        if (!get_user_by_id(client->user_id)->is_moderator) {
            snprintf(msg.text, sizeof(msg.text), "У Вас недостаточно прав для бана пользователей.");
            strcpy(msg.destination, get_user_by_id(client->user_id)->username);
            return msg;
        }
        int user_to_ban_id = find_user_by_name(token);
        if (user_to_ban_id <= 0) {
            snprintf(msg.text, sizeof(msg.text), "Пользователя %s невозможно забанить.", token);
            strcpy(msg.destination, get_user_by_id(client->user_id)->username);
            return msg;
        }
        if (strcmp(destination, GLOBAL_CHAT_NAME) == 0) {
            change_user_global_ban(user_to_ban_id, true);
            strcpy(msg.source, get_user_by_id(client->user_id)->username);
            strcpy(msg.destination, token);
        } else {
            int group_id = find_group_by_name(destination); 
            if (group_id < 0) {
                snprintf(msg.text, sizeof(msg.text), "Группа %s не найдена.", token);
                strcpy(msg.destination, get_user_by_id(client->user_id)->username);
                return msg;
            }   
            ban_user_in_group(user_to_ban_id, group_id);
        } 
        snprintf(msg.text, sizeof(msg.text), "Модератор запретил писать в [%s] пользователю [%s].", destination, token);
        return msg;
    } else if (strcmp(token, "upload") == 0) {
        int error_type = 0;
        long long filesize;
        strcpy(msg.destination, get_user_by_id(client->user_id)->username);
    
        token = strtok(NULL, " ");
        if (token == NULL) {
            error_type = 1;
        } else {
            char *end;
            errno = 0;
            filesize = strtoll(token, &end, 10);
            if (end == token) {
                error_type = 1;
            } else if (*end != '\0') {
                error_type = 1;
            } else if (errno == ERANGE) {
                error_type = 2;
            } else if (filesize > MAX_FILE_SIZE) {
                error_type = 2;
            } else if (filesize <= 0) {
                error_type = 3;
            }
        }
        if (error_type == 0) {
            if (strlen(command) == strlen("upload ") + strlen(token) || strtok(NULL, " ") == NULL) {
                error_type = 1;
            }
        }
        if (error_type == 0) {
            msg.type = FILE_UPLOAD_ACK;
            strcpy(client->pending_filename, command + strlen("upload ") + strlen(token) + 1);
            client->pending_filesize = filesize;
            strcpy(client->pending_token, msg.message_guid);
            client->has_pending_upload = true;
            sprintf(msg.text, "url:/upload/%s|file:%s", msg.message_guid, client->pending_filename);
            add_file_mapping(msg.message_guid, client->pending_filename, get_user_by_id(client->user_id)->username, destination);
            return msg;
        } 
        if (error_type == 1) {
            strcpy(msg.text, "Формат комманды: [<получатель>] /upload <имя файла>.");
            return msg;
        }

        if (error_type == 2) {
            sprintf(msg.text, "Максимальный размер файла для загрузки: %lldМб, размер Вашего файла: %lldМб", MAX_FILE_SIZE / 1024 / 1024, filesize / 1024 / 1024);
            return msg;    
        }
        
        if (error_type == 3) {
            strcpy(msg.text, "Невозможно загрузить пустой файл");
            return msg;
        }
    } else if (strcmp(token, "download") == 0) {
        int error_type = 0;
        file_name_mapping *file = NULL;
        strcpy(msg.destination, get_user_by_id(client->user_id)->username);
    
        token = strtok(NULL, " ");
        if (token == NULL) {
            error_type = 1;
        } else {
            file = get_file_by_sharedname(token);
            if (file == NULL) {
                error_type = 2;
            }
        }
        if (error_type == 0) {
            msg.type = FILE_DOWNLOAD_ACK;
            sprintf(msg.text, "url:/download/%s|file:%s", file->sharedname, file->clientname);
            return msg;
        } 
        if (error_type == 1) {
            strcpy(msg.text, "Формат комманды: /download <guid файла>.");
            return msg;
        }

        if (error_type == 2) {
            sprintf(msg.text, "Файл %s не найден", token);
            return msg;    
        }
    } else {
        // Если команда не распознана, возвращаем текстовое сообщение
        strncpy(msg.text, command, sizeof(msg.text) - 1);
        msg.text[sizeof(msg.text) - 1] = '\0';
        strcpy(msg.destination, get_user_by_id(client->user_id)->username);
    }
    
    // Здесь можно добавить дополнительную обработку команды, если необходимо
    return msg;
}