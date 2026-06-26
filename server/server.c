#include <libwebsockets.h>
#include <string.h>
#include <stdio.h>

#define MAX_MSG_LEN 512
#define MAX_CLIENTS 100

// Структура для хранения данных каждого подключенного клиента
struct client_data {
    struct lws *wsi;
};

// Сессионные данные самого сервера для рассылки сообщений
struct server_storage {
    struct lws *clients[MAX_CLIENTS];
    int client_count;
    char buffer[LWS_PRE + MAX_MSG_LEN]; // Буфер со специальным отступом lws
    size_t msg_len;
};

static struct server_storage chat_server;

// Функция для отправки сообщения всем активным клиентам
void broadcast_message() {
    for (int i = 0; i < chat_server.client_count; i++) {
        if (chat_server.clients[i]) {
            // Запрашиваем у libwebsockets вызов события LWS_CALLBACK_SERVER_WRITEABLE
            lws_callback_on_writable(chat_server.clients[i]);
        }
    }
}

// Главный обработчик событий (Callback) для протокола чата
static int callback_chat(struct lws *wsi, enum lws_callback_reasons reason,
                         void *user, void *in, size_t len) {
    
    struct client_data *vhd = (struct client_data *)user;

    switch (reason) {
        
        // 1. Клиент успешно подключился
        case LWS_CALLBACK_ESTABLISHED:
            if (chat_server.client_count >= MAX_CLIENTS) {
                return -1; // Отклоняем подключение, если превышен лимит
            }
            vhd->wsi = wsi;
            chat_server.clients[chat_server.client_count++] = wsi;
            printf("[Чат] Новый клиент подключен. Всего: %d\n", chat_server.client_count);
            break;

        // 2. Получено сообщение от клиента
        case LWS_CALLBACK_RECEIVE:
            if (len > MAX_MSG_LEN) len = MAX_MSG_LEN;
            
            // Копируем сообщение в буфер, оставляя LWS_PRE байт в начале (требование libwebsockets)
            memcpy(&chat_server.buffer[LWS_PRE], in, len);
            chat_server.msg_len = len;
            
            printf("[Чат] Получено сообщение: %.*s\n", (int)len, (char *)in);
            
            // Запускаем рассылку всем участникам
            broadcast_message();
            break;

        // 3. Сокет готов к отправке данных (вызывается после lws_callback_on_writable)
        case LWS_CALLBACK_SERVER_WRITEABLE:
            if (chat_server.msg_len > 0) {
                lws_write(wsi, (unsigned char *)&chat_server.buffer[LWS_PRE], 
                          chat_server.msg_len, LWS_WRITE_TEXT);
            }
            break;

        // 4. Клиент отключился
        case LWS_CALLBACK_CLOSED:
            for (int i = 0; i < chat_server.client_count; i++) {
                if (chat_server.clients[i] == wsi) {
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
    int port = 8080; // Порт по умолчанию
    
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
