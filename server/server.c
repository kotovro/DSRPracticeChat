#include <libwebsockets.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include "settings.h"
#include "commons.h"
#include "client_login.h"
#include "parser.h"
#include "utils.h"
#include "server_utils.h"
#include "user_storage.h"
#include "group_storage.h"
#include "message_storage.h"

// Хранилище сервера держит указатели на структуры данных клиентов
struct server_storage {
    int client_count;
    struct client_data *clients[MAX_CLIENTS];
    char buffer[LWS_PRE + sizeof(message_format)];
    size_t msg_len;
};

static struct server_storage chat_server;
static char logs_dir[512] = "/tmp/chat_server/";

static pthread_mutex_t lock_tx;
static int force_exit = 0;

// Удаляет \n в конце строки
static void trim_newline(char *s) {
    s[strcspn(s, "\n")] = 0;
}

// Проверка на пустую строку
static int is_empty(const char *s) {
    return s == NULL || s[0] == '\0';
}

static int parse_input(char *input) {
    input = ltrim(rtrim(input));   
    if (input[0] != '/') {
        return 1;
    }
    input++; // пропускаем символ '/'
    if (strcmp(input, "clients") == 0) {
       
        printf("Всего подключено %d клиентов\n", chat_server.client_count);
        for (int i = 0; i < chat_server.client_count; i++) { 
            char ip[128];
            if (lws_get_peer_simple(chat_server.clients[i]->wsi, ip, sizeof(ip)) != NULL) {
                  printf("Клиент %s : ip - %s \n", get_user_by_id(chat_server.clients[i]->user_id)->username, ip);
            }
        } 
        return 0;
    }

    if (strcmp(input, "chats") == 0) {
        printf("%s", "Активные чаты-группы: \n");
        int group_id = iterate_groups(-1);
        while (group_id >= 0) { 
            group_data *group = get_group_by_id(group_id);
            for (int i = 0; i < group->members_count; i++) {
                bool can_break = false;
                for (int j = 0; j < chat_server.client_count; j++) { 
                    if (chat_server.clients[j]->user_id == group->members_list[i]) {
                        printf("%s \n", group->group_name);
                        can_break = true;
                        break;
                    }
                }
                if (can_break) {
                    break;
                }
            }
            group_id = iterate_groups(group_id);
        }
        
        return 0;
    } 

    if (strcmp(input, "exit") == 0) {
        force_exit = 1;
        return 0;
    }
    return 1;
}

// Функция-поток для чтения ввода пользователя из консоли
void *console_input_thread(void *arg) {
    (void)arg;
    char input[MAX_MSG_LEN];

    while (!force_exit) {
        if (fgets(input, sizeof(input), stdin) != NULL) {
            // Удаляем символ переноса строки \n
            trim_newline(input);

            if (is_empty(input))
                continue;

            int error = parse_input(input);
            if (error) {
                printf("Неизвестная команда.\n");
                continue;
            }
        }
    }
    return NULL;
}

void queue_message_for_client(client_data *client, const message_format *msg) {
    if (!client || (client->first_message + client->queue_count) >= MAX_QUEUE) return;

    // Записываем сообщение в текущий свободный слот очереди с отступом LWS_PRE
    int slot = client->first_message + client->queue_count;
    client->queue[slot].message = *msg;
    client->queue_count++;

    // Просим lws вызвать событие WRITEABLE для этого клиента
    lws_callback_on_writable(client->wsi);
}

// Функция для отправки сообщения всем активным клиентам
void broadcast_message(const message_format *message, client_data *exclude) {
    char buf[7];
    snprintf(buf, 6, "%d", exclude ? exclude->user_id : 0);
    printf("Рассылаем сообщение от %s всем клиентам (кроме %s)\n", message->source, exclude ? buf : "никого");
    for (int i = 0; i < chat_server.client_count; i++) {
        if (chat_server.clients[i] && (exclude == NULL || chat_server.clients[i] != exclude)
            && !get_user_by_id(chat_server.clients[i]->user_id)->is_global_chat_disabled) {
            queue_message_for_client(chat_server.clients[i], message);
        }
    }
}

void direct_message(client_data *client, const message_format *message) { 
    queue_message_for_client(client, message);
}

void group_message(group_data* group_info, message_format *message, client_data *exclude) {
    for (int i = 0; i < chat_server.client_count; i++) {
        if (chat_server.clients[i] == exclude) {
            continue;
        }
        for (int j = 0; j < group_info->members_count; j++) {
            if (chat_server.clients[i]->user_id == group_info->members_list[j]) {
                queue_message_for_client(chat_server.clients[i], message);
                break;
            }
        }
    }
}

void route_message(message_format *message, client_data *source, client_data *exclude) {
    bool is_from_server = strcmp(message->source, SERVER_NAME) == 0; 
    if (strcmp(message->destination, GLOBAL_CHAT_NAME) == 0) {
        printf("%s", "will try broadcast \n");
        user_data *user = is_from_server ? NULL : get_user_by_id(source->user_id);
        if (is_from_server || (source->user_id > 0 && user != NULL && !user->is_global_chat_banned)) {
            printf("%s", "will do broadcast \n");
            broadcast_message(message, exclude);
        }
        return;
    }
    int group_id = find_group_by_name(message->destination); 
    if (group_id >= 0) {
        group_data *group = get_group_by_id(group_id);  
        if (is_from_server || !is_user_banned_in_group(source->user_id, group_id)) {
            group_message(group, message, exclude);
        } 
        return; 
    }
    int user_id = find_user_by_name(message->destination);
    if (user_id > 0) {
        for (int i = 0; i < chat_server.client_count; i++) {
            if (chat_server.clients[i]->user_id == user_id) {  
                direct_message(chat_server.clients[i], message);
                break;
            }       
        }
        if (!is_from_server) {
            direct_message(source, message);
        }
        return;
    }
} 

void server_log(char *server_signal) {
    write_to_log(server_signal, logs_dir);
    printf("%s", server_signal);        
}

// Главный обработчик событий (Callback) для протокола чата
static int callback_chat(struct lws *wsi, enum lws_callback_reasons reason,
                         void *user, void *in, size_t len) {
    time_t time_now = time(NULL);
    struct tm *local = localtime(&time_now); 
    char timestamp[20];
    strftime(timestamp, sizeof timestamp, "%Y-%m-%d %H:%M:%S", local);
    // Приводим void *user к нашему типу сессии
    struct client_data *vhd = (struct client_data *)user;
    char log_message[LOG_MESSAGE_LEN];        

    switch (reason) {
        
        // 1. Клиент успешно подключился
         case LWS_CALLBACK_ESTABLISHED:
         {
            char *server_message = "%s Новый клиент подключился. Всего клиентов: %d\n";   
            snprintf(log_message, LOG_MESSAGE_LEN, server_message,
                    timestamp,
                    chat_server.client_count + 1);
            server_log(log_message);
            
            if (chat_server.client_count >= MAX_CLIENTS) {
                server_log("Отказано в подключении");
                return -1;
            }
            vhd->wsi = wsi;
            
            // Сохраняем указатель на сессию в контекст сервера
            chat_server.clients[chat_server.client_count++] = vhd;
            break;
        }
        // 2. Получено сообщение от клиента
        case LWS_CALLBACK_RECEIVE:
        {
            message_format *msg = (message_format *)in;
            char *username = get_user_by_id(vhd->user_id)->username;
            strcpy(msg->source, username); ;
            msg->time_created = time(NULL);
            if (strlen(msg->destination) == 0) {
                strcpy(msg->destination, GLOBAL_CHAT_NAME);
            }
            snprintf(log_message, LOG_MESSAGE_LEN, "%s Получено сообщение {%s} [%s] для [%s]: %s\n",
                    timestamp,
                    msg->message_guid, 
                    msg->source, 
                    msg->destination, 
                    msg->text);
            server_log(log_message);
            save_message(msg);

            if (len != sizeof(message_format)) {
                snprintf(log_message, LOG_MESSAGE_LEN, "%s Ошибка: получено сообщение некорректного размера. Размер: %zu, Ожидаемый размер: %zu\n", timestamp, len, sizeof(message_format));
                server_log(log_message);
                break;
            }

            if (!(vhd->user_id > 0)) {
                int user_id = login(msg);
                printf("user id is: %d \n", user_id);
                if (user_id > 0) { //мы разрешаем только один одновременный логин
                    bool is_already_logged = false;
                    for (int i = 0; i < chat_server.client_count; i++) {
                        if (chat_server.clients[i]->user_id == user_id) {
                            is_already_logged = true;  
                            break;
                        }
                    }
                    if (!is_already_logged) {
                        vhd->user_id = user_id;
                    }
                }
                if (vhd->user_id > 0) {
                    char *username = get_user_by_id(vhd->user_id)->username;
                    char welcome_msg[MAX_NAME_LEN + strlen(WELCOME_MESSAGE) + 1];
                    snprintf(welcome_msg, sizeof(welcome_msg), 
                                                    WELCOME_MESSAGE, 
                                                    username);
                    
                    message_format welcome_msg_struct = create_server_message(LOGIN_SUCCESS, username);
                    strcpy(welcome_msg_struct.text, welcome_msg);
                    direct_message(vhd, &welcome_msg_struct); // Отправляем приветственное сообщение только этому клиенту
                    
                    char join_msg[MAX_NAME_LEN + strlen(JOIN_MESSAGE) + 1];
                    snprintf(join_msg, sizeof(join_msg), 
                                                JOIN_MESSAGE, 
                                                username);
                    welcome_msg_struct.type = TEXT; 
                    strcpy(welcome_msg_struct.text, join_msg);
                    strcpy(welcome_msg_struct.destination, GLOBAL_CHAT_NAME);
                    broadcast_message(&welcome_msg_struct, vhd); // Рассылаем всем клиентам сообщение о входе нового пользователя
                } else {
                    char *username = get_user_by_id(vhd->user_id)->username;
                    char login_fail_msg[MAX_NAME_LEN + strlen(LOGIN_FAIL_MESSAGE) + 1];
                    snprintf(login_fail_msg, sizeof(login_fail_msg), 
                                                    LOGIN_FAIL_MESSAGE);
                    message_format welcome_msg_struct = create_server_message(TEXT, username);
                    strcpy(welcome_msg_struct.text, login_fail_msg);
                    direct_message(vhd, &welcome_msg_struct);
                }
                break;// Не рассылаем эту команду в чат
            }
           
            if (msg->type == TEXT) {
                // Рассылаем сообщение
                route_message(msg, vhd, NULL);
            } else if (msg->type == COMMAND) {
                // Обработка команд
                message_format result = try_execute_command(msg->text, msg->destination, vhd);
                route_message(&result, vhd, NULL); // Отправляем результат
            }
            
            break;
        }
       
        // 3. Сокет готов к отправке данных (вызывается после lws_callback_on_writable)
        case LWS_CALLBACK_SERVER_WRITEABLE:
                if (vhd->queue_count > 0) {
                // Отправляем самое первое сообщение (индекс 0)
                //in data, there will be JSON
                message_format *msg = &(vhd->queue[vhd->first_message].message);
                int amt = lws_write(wsi, (unsigned char *)msg, 
                                    sizeof(message_format), LWS_WRITE_BINARY);
        
                snprintf(log_message, LOG_MESSAGE_LEN, "%s %s сообщение {%s} от %s пользователю %s : %s\n", timestamp,
                    amt < 0 ? "Ошибка отправки:" : "Отправлено",
                    msg->message_guid, 
                    msg->source, 
                    get_user_by_id(vhd->user_id)->username, 
                    msg->text);
                server_log(log_message);
                if (amt < 0) {
                    return -1;
                }
                vhd->queue_count--;
                if (vhd->queue_count == 0) {
                    vhd->first_message = 0;
                } else {
                    vhd->first_message++;
                }

                // ВАЖНО: Если в очереди ОСТАЛИСЬ сообщения, просим lws вызвать 
                // событие WRITEABLE снова на следующем итерации цикла event loop!
                if (vhd->queue_count > 0) {
                    lws_callback_on_writable(wsi);
                }
            }
            break;

        // 4. Клиент отключился
        case LWS_CALLBACK_CLOSED:
            for (int i = 0; i < chat_server.client_count; i++) {
                if (chat_server.clients[i]->wsi == wsi) {
                    // Удаляем клиента из списка смещением массива
                    chat_server.clients[i] = chat_server.clients[chat_server.client_count - 1];
                    chat_server.client_count--;
                    break;
                }
            }
            snprintf(log_message, LOG_MESSAGE_LEN, "Клиент %s отключился. Осталось: %d\n", get_user_by_id(vhd->user_id)->username, chat_server.client_count);
            server_log(log_message);
            
            break;

        default:
            break;
    }
    return 0;
}

// Определение поддерживаемых протоколов
static struct lws_protocols protocols[] = {
    { 
        .name = "chat-protocol", 
        .callback = callback_chat, 
        .per_session_data_size = sizeof(struct client_data), 
        .rx_buffer_size = sizeof(message_format), // Размер буфера приема равен размеру структуры сообщения
        .id = 0,               // Явно инициализируем ID протокола
        .user = NULL,          // Дополнительный указатель пользователя
        .tx_packet_size = 0    // Размер пакета отправки по умолчанию
    },
    { NULL, NULL, 0, 0, 0, NULL, 0 } // Явный маркер конца списка для всех полей
};

int main(int argc, char **argv) {
    init_user_storage();
    struct lws_context_creation_info info;
    struct lws_context *context;
    pthread_t input_th;
    int opt;
    int port = SERVER_PORT; // Порт по умолчанию
    
    while ((opt = getopt(argc, argv, "p:d:h")) != -1) {
        switch (opt) {
            case 'p':
                port = atoi(optarg);
                if (port <= 0 || port > 65535) {
                    fprintf(stderr, "Неверный порт: %s\n", optarg);
                    return 1;
                }
                break;
            case 'd':
                if (!is_directory(optarg)) {
                    printf("Директории с адресом %s не существует, логи пишем в %s\n", optarg, logs_dir);
                } else {
                    strcpy(logs_dir, optarg);
                }
                break;  
            case 'h':
            default:
                fprintf(stderr, "Использование: %s [-p server_port] [-d logs_directory]\n", argv[0]);
                return opt == 'h' ? 0 : 1;
        }
    }

    pthread_mutex_init(&lock_tx, NULL);

    memset(&info, 0, sizeof(info));
    info.port = port;                // Порт сервера
    info.protocols = protocols;      // Массив протоколов
    info.gid = -1;
    info.uid = -1;

    mkdir_p(logs_dir, 0755);
    // Создаем контекст сервера
    context = lws_create_context(&info);
    if (!context) {
        fprintf(stderr, "Ошибка инициализации libwebsockets\n");
        
        return 1;
    }

    // Запускаем поток для чтения ввода пользователя
    if (pthread_create(&input_th, NULL, console_input_thread, NULL) != 0) {
        fprintf(stderr, "Ошибка создания потока ввода\n");
        lws_context_destroy(context);
        return 1;
    }

    printf("Сервер чата запущен на порту %d...\n", port);

    // Главный бесконечный цикл обработки событий (Event Loop)
    while (!force_exit) {
        // Таймаут 50 мс означает частоту опроса сокетов
        lws_service(context, 50);
    }

    pthread_cancel(input_th);
    pthread_join(input_th, NULL);
    pthread_mutex_destroy(&lock_tx);
    lws_context_destroy(context);
    return 0;
}