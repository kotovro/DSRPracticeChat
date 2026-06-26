#include <libwebsockets.h>
#include <string.h>
#include <stdio.h>
#include "../common/settings.h"
#include "../common/commons.h"
#include "client_login.h"

// Хранилище сервера держит указатели на структуры данных клиентов
struct server_storage {
    struct client_data *clients[MAX_CLIENTS];
    int client_count;
    char buffer[LWS_PRE + MAX_NAME_LEN + MAX_MSG_LEN + 4]; // Запас под "Имя: Текст"
    size_t msg_len;
};

static struct server_storage chat_server;


void queue_message_for_client(struct client_data *client, const char *text, size_t len) {
    if (!client || client->queue_count >= MAX_QUEUE) return;

    if (len > MAX_MSG_LEN) len = MAX_MSG_LEN;

    // Записываем сообщение в текущий свободный слот очереди с отступом LWS_PRE
    int slot = client->queue_count;
    memcpy(&client->queue[slot].data[LWS_PRE], text, len);
    client->queue[slot].len = len;
    client->queue_count++;

    // Просим lws вызвать событие WRITEABLE для этого клиента
    lws_callback_on_writable(client->wsi);
}

// Функция для отправки сообщения всем активным клиентам
void broadcast_message(const char *message, size_t len, struct client_data *exclude) {
    // Копируем результат в буфер рассылки с отступом LWS_PRE
    memcpy(&chat_server.buffer[LWS_PRE], message, len);
    chat_server.msg_len = len;

    for (int i = 0; i < chat_server.client_count; i++) {
        if (chat_server.clients[i] && (exclude == NULL || chat_server.clients[i] != exclude)) {
            queue_message_for_client(chat_server.clients[i], message, len);
        }
    }
}

void direct_message(struct client_data *recipient, const char *message, size_t len) { 
    // Копируем результат в буфер рассылки с отступом LWS_PRE
    memcpy(&chat_server.buffer[LWS_PRE], message, len);
    chat_server.msg_len = len;
    queue_message_for_client(recipient, message, len);
}




// Главный обработчик событий (Callback) для протокола чата
static int callback_chat(struct lws *wsi, enum lws_callback_reasons reason,
                         void *user, void *in, size_t len) {
    
    // Приводим void *user к нашему типу сессии
    struct client_data *vhd = (struct client_data *)user;

    switch (reason) {
        
        // 1. Клиент успешно подключился
         case LWS_CALLBACK_ESTABLISHED:
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
            // Проверяем: если сообщение начинается со спец-команды "/login ", меняем ник
            if (!vhd->is_logged_in) {
                if (login((char *)in, len, vhd)) {
                    char welcome_msg[MAX_NAME_LEN + strlen(WELCOME_MESSAGE) + 1];
                    int welcome_len = snprintf(welcome_msg, sizeof(welcome_msg), 
                                                    WELCOME_MESSAGE, vhd->username);
                    direct_message(vhd, welcome_msg, welcome_len); // Отправляем приветственное сообщение только этому клиенту
                    
                    char join_msg[MAX_NAME_LEN + strlen(JOIN_MESSAGE) + 1];
                    int join_len = snprintf(join_msg, sizeof(join_msg), 
                                                    JOIN_MESSAGE, vhd->username);
                    broadcast_message(join_msg, join_len, vhd); // Рассылаем всем клиентам сообщение о входе нового пользователя
                }
                break;// Не рассылаем эту команду в чат
            }

            // Для обычного сообщения склеиваем строку: "Имя: Текст"
            char formatted_msg[MAX_NAME_LEN + MAX_MSG_LEN + 3];
            int formatted_len = snprintf(formatted_msg, sizeof(formatted_msg), 
                                         "%s: %.*s", vhd->username, (int)len, (char *)in);
            
            printf("[Чат] Получено сообщение: %.*s\n", (int)formatted_len, (char *)formatted_msg);

            // Рассылаем всем клиентам, включая отправителя
            broadcast_message(formatted_msg, formatted_len, NULL);
            break;
       

        // 3. Сокет готов к отправке данных (вызывается после lws_callback_on_writable)
        case LWS_CALLBACK_SERVER_WRITEABLE:
               if (vhd->queue_count > 0) {
                // Отправляем самое первое сообщение (индекс 0)
                int amt = lws_write(wsi, (unsigned char *)&vhd->queue[0].data[LWS_PRE], 
                                    vhd->queue[0].len, LWS_WRITE_TEXT);
                
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
            printf("[Чат] Клиент отключился. Осталось: %d\n", chat_server.client_count);
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
        .rx_buffer_size = MAX_MSG_LEN,
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
