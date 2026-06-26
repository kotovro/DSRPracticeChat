#include <libwebsockets.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <stdbool.h>
#include "../common/settings.h"

static bool is_logged_in = false; // Флаг для проверки, вошел ли пользователь в чат
// Глобальные переменные для управления состоянием
static struct lws *web_socket = NULL;
static int force_exit = 0;

// Буфер для отправки сообщений (с обязательным отступом LWS_PRE)
static char tx_buffer[LWS_PRE + MAX_MSG_LEN];
static size_t tx_msg_len = 0;
static pthread_mutex_t lock_tx; // Мутекс для защиты буфера отправки

// Функция-поток для чтения ввода пользователя из консоли
void *console_input_thread(void *arg) {
    (void)arg;
    char input[MAX_MSG_LEN];

    printf("Вы вошли в чат. Введите сообщение и нажмите Enter:\n");

    while (!force_exit) {
        if (fgets(input, sizeof(input), stdin) != NULL) {
            // Удаляем символ переноса строки \n
            input[strcspn(input, "\n")] = 0;

            if (strlen(input) == 0) continue;

            // Защищаем буфер и копируем туда данные
            pthread_mutex_lock(&lock_tx);
            tx_msg_len = strlen(input);
            memcpy(&tx_buffer[LWS_PRE], input, tx_msg_len);
            pthread_mutex_unlock(&lock_tx);

            // Сигнализируем libwebsockets, что мы готовы отправить данные
            if (web_socket) {
                lws_callback_on_writable(web_socket);
            }
        }
    }
    return NULL;
}

// Callback-обработчик событий клиента
static int callback_chat_client(struct lws *wsi, enum lws_callback_reasons reason,
                                void *user, void *in, size_t len) {
    (void)user;

    switch (reason) {
        // 1. Успешное подключение к серверу
        case LWS_CALLBACK_CLIENT_ESTABLISHED:
            web_socket = wsi;
            printf("[Система] Подключено! Пожалуйста, зарегистрируйтесь (/login <имя>)\n");
            
            break;

        // 2. Получено сообщение от сервера (из общего чата)
        case LWS_CALLBACK_CLIENT_RECEIVE:
            printf("[Чат]: %.*s\n", (int)len, (char *)in);
            break;

        // 3. Сокет готов отправить данные в сеть
        case LWS_CALLBACK_CLIENT_WRITEABLE:
            if (!is_logged_in) {
                // Если пользователь еще не вошел в чат, отправляем команду регистрации
                pthread_mutex_lock(&lock_tx);
                if (tx_msg_len > 0 && strncmp(&tx_buffer[LWS_PRE], "/login ", 7) == 0) {
                    lws_write(wsi, (unsigned char *)&tx_buffer[LWS_PRE], tx_msg_len, LWS_WRITE_TEXT);
                    tx_msg_len = 0; // Сбрасываем длину после отправки
                    is_logged_in = true; // Отметим, что пользователь вошел
                }
                else {
                    printf("[Система] Пожалуйста, зарегистрируйтесь с помощью команды: /login <имя>\n");
                }
                pthread_mutex_unlock(&lock_tx);
            } else {
                // Если пользователь уже вошел, отправляем обычные сообщения
                pthread_mutex_lock(&lock_tx);
                if (tx_msg_len > 0) {
                    lws_write(wsi, (unsigned char *)&tx_buffer[LWS_PRE], tx_msg_len, LWS_WRITE_TEXT);
                    tx_msg_len = 0; // Сбрасываем длину после отправки
                }
                pthread_mutex_unlock(&lock_tx);
            }
            break;

        // 4. Соединение закрыто или произошла ошибка
        case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
            fprintf(stderr, "[Ошибка] Не удалось подключиться к серверу.\n");
            force_exit = 1;
            break;

        case LWS_CALLBACK_CLIENT_CLOSED:
            printf("[Система] Соединение закрыто.\n");
            force_exit = 1;
            break;

        default:
            break;
    }
    return 0;
}

// Регистрация протокола (имя должно строго совпадать с серверным)
static struct lws_protocols protocols[] = {
    {
        .name = "chat-protocol",
        .callback = callback_chat_client,
        .per_session_data_size = 0,
        .rx_buffer_size = MAX_MSG_LEN,
        .id = 0,
        .user = NULL,
        .tx_packet_size = 0
    },
    { NULL, NULL, 0, 0, 0, NULL, 0 }
};

int main(int argc, char **argv) {
    struct lws_context_creation_info info;
    struct lws_context *context;
    struct lws_client_connect_info ccinfo;
    pthread_t input_th;
    const char *server_address = SERVER_IP; // IP сервера по умолчанию
    int server_port = SERVER_PORT; // Порт по умолчанию
    int opt;

    while ((opt = getopt(argc, argv, "s:p:h")) != -1) {
        switch (opt) {
            case 's':
                server_address = optarg;
                break;
            case 'p':
                server_port = atoi(optarg);
                if (server_port <= 0 || server_port > 65535) {
                    fprintf(stderr, "Неверный порт: %s\n", optarg);
                    return 1;
                }
                break;
            case 'h':
            default:
                fprintf(stderr, "Использование: %s [-s server_ip] [-p server_port]\n", argv[0]);
                return opt == 'h' ? 0 : 1;
        }
    }

    pthread_mutex_init(&lock_tx, NULL);

    // Настройка контекста клиента
    memset(&info, 0, sizeof(info));
    info.port = CONTEXT_PORT_NO_LISTEN; // Клиент не слушает порты
    info.protocols = protocols;
    info.gid = -1;
    info.uid = -1;

    context = lws_create_context(&info);
    if (!context) {
        fprintf(stderr, "Ошибка создания контекста libwebsockets\n");
        return 1;
    }

    // Настройка параметров подключения к серверу
    memset(&ccinfo, 0, sizeof(ccinfo));
    ccinfo.context = context;
    ccinfo.address = server_address;       // IP сервера
    ccinfo.port = server_port;             // Порт сервера
    ccinfo.path = "/";
    ccinfo.host = ccinfo.address;
    ccinfo.origin = ccinfo.address;
    ccinfo.protocol = protocols[0].name; // "chat-protocol"

    // Инициируем подключение
    if (!lws_client_connect_via_info(&ccinfo)) {
        fprintf(stderr, "Ошибка инициализации подключения\n");
        lws_context_destroy(context);
        return 1;
    }

    // Запускаем поток для чтения ввода пользователя
    if (pthread_create(&input_th, NULL, console_input_thread, NULL) != 0) {
        fprintf(stderr, "Ошибка создания потока ввода\n");
        lws_context_destroy(context);
        return 1;
    }

    // Главный цикл обработки сетевых событий
    while (!force_exit) {
        // Опрашиваем события сети каждые 50 мс
        if (lws_service(context, 50) < 0) {
            break;
        }
    }

    // Завершение работы
    pthread_cancel(input_th);
    pthread_join(input_th, NULL);
    pthread_mutex_destroy(&lock_tx);
    lws_context_destroy(context);

    printf("Работа клиента завершена.\n");
    return 0;
}
