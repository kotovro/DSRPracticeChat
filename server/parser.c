#include "parser.h" 
#include "settings.h"
#include "utils.h"
#include "server_utils.h"
#include "user_storage.h"
#include "group_storage.h"

message_format try_execute_command(const char *command, const char *destination, client_data *client) {    
    message_format msg = create_server_message(TEXT, destination); 
    char copy_command[strlen(command) + 1];
    strcpy(copy_command, command);

    char *token = strtok(copy_command, " ");
    if (strcmp(token, "login") == 0) {
        // msg.type = COMMAND;
        // strncpy(msg.text, command, sizeof(msg.text) - 1);
        // msg.text[sizeof(msg.text) - 1] = '\0';
    } else if (strcmp(token, "mute") == 0) {
        if (strcmp(destination, GLOBAL_CHAT_NAME) == 0) {
            change_user_mute(client->user_id, true);
            snprintf(msg.text, sizeof(msg.text), "Теперь Вы не будете видеть сообщения, которые приходят в %s.", GLOBAL_CHAT_NAME);
        }
    } else if (strcmp(token, "unmute") == 0) {
        if (strcmp(destination, GLOBAL_CHAT_NAME) == 0) {
            change_user_mute(client->user_id, false);
            snprintf(msg.text, sizeof(msg.text), "Теперь Вы снова можете видеть сообщения, которые приходят в %s.", GLOBAL_CHAT_NAME);
        }
    } else if (strcmp(token, "add_group") == 0) {
        token = strtok(NULL, " ");
        if (token == NULL) {
            snprintf(msg.text, sizeof(msg.text), "Формат команды: /add_group <название группы>.");
            return msg;
        }
        if (find_user_by_name(token) >= 0) {
            snprintf(msg.text, sizeof(msg.text), "Группу %s создать невозможно.", token);
            return msg;
        }
        int res = create_group(token);
        if (res < 0) {
            snprintf(msg.text, sizeof(msg.text), "Группу %s создать невозможно.", token);
            return msg;
        } 
        add_user_to_group(client->user_id, res);
        snprintf(msg.text, sizeof(msg.text), "Группа %s успешно создана.", token);
        strcpy(msg.destination, token);
    } else if (strcmp(token, "add_user") == 0) {
        int group_id = find_group_by_name(destination); 
        if (group_id < 0) {
            snprintf(msg.text, sizeof(msg.text), "Группа %s не найдена.", token);
            return msg;
        }   
        if (!is_user_in_group(client->user_id, group_id)) {
            strcpy(msg.text, "Недостаточно прав для выполнения этой команды");
            return msg;
        } 
        token = strtok(NULL, " ");
        if (token == NULL) {
            snprintf(msg.text, sizeof(msg.text), "Формат команды: [<название группы>] /add_user <имя пользователя>.");
            return msg;
        }  
        int user_id = find_user_by_name(token);
        if (user_id <= 0) {
            snprintf(msg.text, sizeof(msg.text), "Пользователь %s не найден.", token);
            return msg;
        } 
        if (add_user_to_group(user_id, group_id)) {
            snprintf(msg.text, sizeof(msg.text), "Пользователь %s добавлен в группу.", token);
            return msg;
        } 
        snprintf(msg.text, sizeof(msg.text), "Пользователя %s невозможно добавить в группу.", token);
        return msg;
    } else {
        // Если команда не распознана, возвращаем текстовое сообщение
        strncpy(msg.text, command, sizeof(msg.text) - 1);
        msg.text[sizeof(msg.text) - 1] = '\0';
    }
    
    // Здесь можно добавить дополнительную обработку команды, если необходимо
    return msg;
}