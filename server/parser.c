#include "parser.h" 
#include "settings.h"
#include "utils.h"
#include "server_utils.h"
#include "user_storage.h"

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
    } else {
        // Если команда не распознана, возвращаем текстовое сообщение
        strncpy(msg.text, command, sizeof(msg.text) - 1);
        msg.text[sizeof(msg.text) - 1] = '\0';
    }
    
    // Здесь можно добавить дополнительную обработку команды, если необходимо
    return msg;
}