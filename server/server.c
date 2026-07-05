#include <libwebsockets.h>
#include <string.h>
#include <stdio.h>
#include "settings.h"
#include "commons.h"
#include "client_login.h"
#include "parser.h"
#include "utils.h"
#include "server_utils.h"

// Хранилище сервера держит указатели на структуры данных клиентов
struct server_storage {
    struct client_data *clients[MAX_CLIENTS];
    struct group_data *groups[MAX_GROUPS];
    int client_count;
    char buffer[LWS_PRE + sizeof(message_format)];
    size_t msg_len;
};

static struct server_storage chat_server;


void queue_message_for_client(struct client_data *client, const message_format *msg) {
    if (!client || client->queue_count >= MAX_QUEUE) return;

    // Записываем сообщение в текущий свободный слот очереди с отступом LWS_PRE
    int slot = client->queue_count;
    memcpy(&client->queue[slot].text[LWS_PRE], msg, sizeof(message_format));
    client->queue_count++;

    // Просим lws вызвать событие WRITEABLE для этого клиента
    lws_callback_on_writable(client->wsi);
}

// Функция для отправки сообщения всем активным клиентам
void broadcast_message(const message_format *message, struct client_data *exclude) {
    printf("Рассылаем сообщение от %s всем клиентам (кроме %s)\n", message->source, exclude ? exclude->username : "никого");
    for (int i = 0; i < chat_server.client_count; i++) {
        if (chat_server.clients[i] && (exclude == NULL || chat_server.clients[i] != exclude)
            && !chat_server.clients[i]->is_global_chat_disabled) {
            queue_message_for_client(chat_server.clients[i], message);
        }
    }
}

void direct_message(struct client_data *recipient, const message_format *message) { ///think to hpo =to send to clients; most lilkel; make sirect to 
    queue_message_for_client(recipient, message);
}


// Главный обработчик событий (Callback) для протокола чата
static int callback_chat(struct lws *wsi, enum lws_callback_reasons reason,
                         void *user, void *in, size_t len) {
    
    // Приводим void *user к нашему типу сессии
    struct client_data *vhd = (struct client_data *)user;

    switch (reason) {
        
        // 1. Клиент успешно подключился
         case LWS_CALLBACK_ESTABLISHED:
            printf("Новый клиент подключился. Всего клиентов: %d\n", chat_server.client_count + 1);
            if (chat_server.client_count >= MAX_CLIENTS) {
                return -1;
            }
            vhd->wsi = wsi;
            // Присваиваем временное имя, пока клиент не прислал свое
            snprintf(vhd->username, MAX_NAME_LEN, "User_%d", chat_server.client_count + 1);
            
            // Сохраняем указатель на сессию в общий список сервера
            chat_server.clients[chat_server.client_count++] = vhd;
            break;


        // 2. Получено сообщение от клиента
        case LWS_CALLBACK_RECEIVE:
        {
            if (len < sizeof(message_format)) {
                printf("Ошибка: получено сообщение меньшего размера, чем ожидается. Размер: %zu, Ожидаемый размер: %zu\n", len, sizeof(message_format));
                // break;
            }
            message_format *msg = (message_format *)in;
            if (!vhd->is_logged_in) {
                if (login(msg, vhd)) {
                    char welcome_msg[MAX_NAME_LEN + strlen(WELCOME_MESSAGE) + 1];
                    snprintf(welcome_msg, sizeof(welcome_msg), 
                                                    WELCOME_MESSAGE, vhd->username);
                    message_format welcome_msg_struct = create_server_message(LOGIN_SUCCESS, vhd->username);
                    strcpy(welcome_msg_struct.text, welcome_msg);
                    direct_message(vhd, &welcome_msg_struct); // Отправляем приветственное сообщение только этому клиенту
                    
                    
                    char join_msg[MAX_NAME_LEN + strlen(JOIN_MESSAGE) + 1];
                    snprintf(join_msg, sizeof(join_msg), 
                                                    JOIN_MESSAGE, vhd->username);
                    welcome_msg_struct.type = TEXT; 
                    strcpy(welcome_msg_struct.text, join_msg);
                    strcpy(welcome_msg_struct.destination, GLOBAL_CHAT_NAME);
                    broadcast_message(&welcome_msg_struct, vhd); // Рассылаем всем клиентам сообщение о входе нового пользователя
                } else {
                    char login_fail_msg[MAX_NAME_LEN + strlen(LOGIN_FAIL_MESSAGE) + 1];
                    snprintf(login_fail_msg, sizeof(login_fail_msg), 
                                                    LOGIN_FAIL_MESSAGE);
                    message_format welcome_msg_struct = create_server_message(TEXT, vhd->username);
                    strcpy(welcome_msg_struct.text, login_fail_msg);
                    direct_message(vhd, &welcome_msg_struct);
                }
                break;// Не рассылаем эту команду в чат
            }

            strcpy(msg->source, vhd->username); 
            generate_uuid(msg->message_guid);
            msg->time_created = time(NULL);
            if (strlen(msg->destination) == 0) {
                strcpy(msg->destination, GLOBAL_CHAT_NAME);
            }

            if (msg->type == TEXT) {
                  
               printf("Получено сообщение от  %s для %s : %s\n", msg->source, msg->destination, msg->text);

                // Рассылаем всем клиентам, включая отправителя
                broadcast_message(msg, NULL);
            } else if (msg->type == COMMAND) {
                // Обработка команд
                message_format result = try_execute_command(msg->text, msg->destination, vhd);
                direct_message(vhd, &result); // Отправляем команду обратно клиенту, чтобы он видел результат
            }
            
            break;
        }
       
        // 3. Сокет готов к отправке данных (вызывается после lws_callback_on_writable)
        case LWS_CALLBACK_SERVER_WRITEABLE:
                if (vhd->queue_count > 0) {
                // Отправляем самое первое сообщение (индекс 0)
                //in data, there will be JSON
                int amt = lws_write(wsi, (unsigned char *)&vhd->queue[0].text[LWS_PRE], 
                                    sizeof(message_format), LWS_WRITE_BINARY);
                if (amt < 0) return -1; // Ошибка отправки, закрываем соединение

                // Сдвигаем очередь влево (удаляем отправленное сообщение)
                if (vhd->queue_count > 1) {
                    vhd->queue[0] = vhd->queue[1];
                }
                vhd->queue_count--;

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
            printf("Клиент отключился. Осталось: %d\n", chat_server.client_count);
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
    struct lws_context_creation_info info;
    struct lws_context *context;
    int opt;
    int port = SERVER_PORT; // Порт по умолчанию
    
    while ((opt = getopt(argc, argv, "p:h")) != -1) {
        switch (opt) {
            case 'p':
                port = atoi(optarg);
                if (port <= 0 || port > 65535) {
                    fprintf(stderr, "Неверный порт: %s\n", optarg);
                    return 1;
                }
                break;
            case 'h':
            default:
                fprintf(stderr, "Использование: %s [-p server_port]\n", argv[0]);
                return opt == 'h' ? 0 : 1;
        }
    }


    memset(&info, 0, sizeof(info));
    info.port = port;                // Порт сервера
    info.protocols = protocols;      // Массив протоколов
    info.gid = -1;
    info.uid = -1;

    // Создаем контекст сервера
    context = lws_create_context(&info);
    if (!context) {
        fprintf(stderr, "Ошибка инициализации libwebsockets\n");
        return 1;
    }

    printf("Сервер чата запущен на порту %d...\n", port);

    // Главный бесконечный цикл обработки событий (Event Loop)
    while (1) {
        // Таймаут 50 мс означает частоту опроса сокетов
        lws_service(context, 50);
    }

    lws_context_destroy(context);
    return 0;
}