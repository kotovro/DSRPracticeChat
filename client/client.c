#include <libwebsockets.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <stdbool.h>
#include "settings.h"
#include "commons.h"
#include "utils.h"

// Глобальные переменные для управления состоянием
static struct lws *web_socket = NULL;
static int force_exit = 0;
static char logs_dir[512] = "/tmp/chat_client/";

// Буфер для отправки сообщений (с обязательным отступом LWS_PRE)
static char tx_buffer[LWS_PRE + sizeof(message_format)];
static pthread_mutex_t lock_tx; // Мутекс для защиты буфера отправки

// Удаляет \n в конце строки
static void trim_newline(char *s) {
    s[strcspn(s, "\n")] = 0;
}

// Проверка на пустую строку
static int is_empty(const char *s) {
    return s == NULL || s[0] == '\0';
}

static int parse_input(char *input, message_format *msg) {
    msg->type = TEXT; // по умолчанию считаем, что это текстовое сообщение

    input = ltrim(rtrim(input));
    int error =  extract_bracket_word(input, msg->destination, sizeof(msg->destination) - 1);
    if (error)
    {
        return 1; // ошибка при извлечении области применения
    }

    int offset = strlen(msg->destination); 
    if (offset > 0) {
        input = ltrim(input + offset + 2);
    }
    
    if (input[0] == '/') {
        msg->type = COMMAND;
        input++; // пропускаем символ '/'
    }
    strncpy(msg->text, input, sizeof(msg->text) - 1);
    msg->text[sizeof(msg->text) - 1] = '\0';
  
    return 0;
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

            // парсинг
            message_format msg = {0};
            int error = parse_input(input, &msg);
            if (error) {
                printf("Неверный формат сообщения.\n");
                continue;
            }
    
            // Защищаем буфер и копируем туда данные
            pthread_mutex_lock(&lock_tx);
            memcpy(&tx_buffer[LWS_PRE], &msg, sizeof(message_format));
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
            printf("Подключено! Пожалуйста, зарегистрируйтесь (/login <имя>)\n");
            
            break;

        // 2. Получено сообщение от сервера (из общего чата)
        case LWS_CALLBACK_CLIENT_RECEIVE:
        {
            if (len < sizeof(message_format)) {
                // return 0; // Игнорируем сообщения, которые не совпдаюат по формату
            }
            message_format *msg = (message_format *)in;
            
            struct tm *local = localtime(&msg->time_created); 
            
            char log_message[LOG_MESSAGE_LEN];
            char timestamp[20];
            //check fort date modfied
            strftime(timestamp, sizeof timestamp,
                "%Y-%m-%d %H:%M:%S", local);
            snprintf(log_message, LOG_MESSAGE_LEN, "%s %s {%s} [%s] для [%s]: %s\n",
                    timestamp,
                    msg->time_deleted > 0
                        ? "Удалено сообщение"  
                        : msg->time_modified > 0 
                            ? "Изменено сообщение" 
                            : "", 
                    msg->message_guid, 
                    msg->source, 
                    msg->destination, 
                    msg->time_deleted > 0 ? "" : msg->text);
            printf("%s", log_message);
            write_to_log(log_message, logs_dir);

            break;
        }
        // 3. Сокет готов отправить данные в сеть
        case LWS_CALLBACK_CLIENT_WRITEABLE:
        {
            const message_format *raw = (const message_format *)&tx_buffer[LWS_PRE];
            if (strlen(raw->text) == 0) {
                break; // если текст пустой, ничего не отправляем
            }
            pthread_mutex_lock(&lock_tx);
            lws_write(wsi,
                    (unsigned char *)&tx_buffer[LWS_PRE],
                    sizeof(message_format),
                    LWS_WRITE_BINARY);   
            pthread_mutex_unlock(&lock_tx);
            break;
        }
        // 4. Соединение закрыто или произошла ошибка
        case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
            fprintf(stderr, "Ошибка: Не удалось подключиться к серверу.\n");
            force_exit = 1;
            break;

        case LWS_CALLBACK_CLIENT_CLOSED:
            printf("Соединение закрыто.\n");
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
        .rx_buffer_size = sizeof(message_format),
        .id = 0,
        .user = NULL,
        .tx_packet_size = 0
    },
    { NULL, NULL, 0, 0, 0, NULL, 0 }
};

static const struct lws_extension extensions[] = {
    {
        "permessage-deflate",
        lws_extension_callback_pm_deflate, // Встроенный callback сжатия в LWS
        "permessage-deflate; client_no_context_takeover; client_max_window_bits"
    },
    { NULL, NULL, NULL /* конец списка */ }
};

int main(int argc, char **argv) {
    struct lws_context_creation_info info;
    struct lws_context *context;
    struct lws_client_connect_info ccinfo;
    pthread_t input_th;
    const char *server_address = SERVER_IP; // IP сервера по умолчанию
    int server_port = SERVER_PORT; // Порт по умолчанию
    int opt;

    while ((opt = getopt(argc, argv, "s:p:d:h")) != -1) {
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
            case 'd':
                if (!is_directory(optarg)) {
                    printf("Директории с адресом %s не существует, логи пишем в %s\n", optarg, logs_dir);
                } else {
                    strcpy(logs_dir, optarg);
                }
                break;    
            case 'h':
            default:
                fprintf(stderr, "Использование: %s [-s server_ip] [-p server_port] [-d logs_directory]\n", argv[0]);
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

    mkdir_p(logs_dir, 0755);
    // Настройка параметров подключения к серверу
    memset(&ccinfo, 0, sizeof(ccinfo));
    ccinfo.context = context;
    ccinfo.address = server_address;       // IP сервера
    ccinfo.port = server_port;             // Порт сервера
    ccinfo.path = "/";
    ccinfo.host = ccinfo.address;
    ccinfo.origin = ccinfo.address;
    ccinfo.protocol = protocols[0].name; // "chat-protocol"
    ccinfo.client_exts = extensions; 

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
