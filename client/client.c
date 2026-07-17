#include <libwebsockets.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <stdbool.h>
#include <sys/stat.h>   
#include "settings.h"
#include "commons.h"
#include "utils.h"

#define CHUNK_SIZE 4096

// Структура для хранения состояния отправки файла на клиенте
typedef struct {
    FILE *fp;
    size_t total_size;
    size_t sent_size;
    char filename[MAX_FILENAME_LEN];
} client_file_upload_data;

// Глобальные переменные для управления состоянием
static struct lws *web_socket = NULL;
static int force_exit = 0;
static char logs_dir[512] = "/tmp/chat_client/";
char server_address[512] = SERVER_IP; // IP сервера по умолчанию
int server_port = SERVER_PORT; // Порт по умолчанию

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
        char input_copy[MAX_MSG_LEN];
        strcpy(input_copy, input); 
        char *token = strtok(input_copy, " ");
        if (strcmp(token, "upload") == 0) { 
            char *filename = strtok(NULL, " ");
            struct stat st;

            if (stat(filename, &st) == 0) {
                sprintf(input, "upload %ld %s", st.st_size, filename);
            } else {

                return 1;
            }
        }
    }
    strcpy(msg->text, input);
  
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

// Callback-обработчик событий HTTP-клиента
static int callback_http_client(struct lws *wsi, enum lws_callback_reasons reason,
                                void *user, void *in, size_t len) {
    (void)user;

    client_file_upload_data *upload = (client_file_upload_data *)lws_wsi_user(wsi);
    
    // Буфер для отправки данных (обязательно резервируем LWS_PRE в начале и в конце)
    uint8_t buf[LWS_PRE + CHUNK_SIZE + LWS_PRE];
    uint8_t *p = &buf[LWS_PRE];
    char log_message[LOG_MESSAGE_LEN];
    char timestamp[20];
    time_t now = time(NULL);
    struct tm *local = localtime(&now); 
    strftime(timestamp, sizeof timestamp, "%Y-%m-%d %H:%M:%S", local);

    switch (reason) {
         case LWS_CALLBACK_CLIENT_APPEND_HANDSHAKE_HEADER: {
            // LWS передает указатель на указатель текущей позиции в буфере заголовков
            unsigned char **p = (unsigned char **)in;
            unsigned char *end = (*p) + len - 1; // Конец доступного буфера

            // 1. Добавляем заголовок Content-Length
            char len_str[32];
            int len_str_len = snprintf(len_str, sizeof(len_str), "%zu", upload->total_size);
            
            if (lws_add_http_header_by_token(wsi, WSI_TOKEN_HTTP_CONTENT_LENGTH, 
                                             (unsigned char *)len_str, len_str_len, p, end)) {
                sprintf(log_message, "%s Не удалось добавить заголовок Content-Length\n", timestamp);
                write_to_log(log_message, logs_dir);    
                return -1;
            }

            // 2. Добавляем заголовок Content-Type
            if (lws_add_http_header_by_token(wsi, WSI_TOKEN_HTTP_CONTENT_TYPE, 
                                             (unsigned char *)"application/octet-stream", 24, p, end)) {
                sprintf(log_message, "%s Не удалось добавить заголовок Content-Type\n", timestamp);
                write_to_log(log_message, logs_dir);    
                return -1;
            }

            //необходимо пнуть lws для того, чтобы была начата отправка данных 
            lws_client_http_body_pending(wsi, 1); 
            lws_callback_on_writable(wsi); 
            return 0;
        }

        case LWS_CALLBACK_CLIENT_HTTP_WRITEABLE:
            if (!upload || !upload->fp) return -1;


            // 1. Читаем очередной кусок файла с диска
            size_t n = fread(p, 1, CHUNK_SIZE, upload->fp);
            if (n > 0) {
                upload->sent_size += n;
                
                // Проверяем, последний ли это чанк
                int is_last = (upload->sent_size >= upload->total_size || feof(upload->fp));
                
                // Формируем флаги отправки HTTP-тела
                enum lws_write_protocol flags = lws_write_ws_flags(
                    LWS_WRITE_HTTP, 
                    upload->sent_size == n, // Посылаем ли мы самый первый кусок?
                    is_last                 // Посылаем ли мы финальный кусок?
                );

                // 2. Отправляем кусок в сеть
                if (lws_write(wsi, p, n, flags) < 0) {
                    sprintf(log_message, "%s Ошибка отправки файла %s на сервер\n", timestamp, upload->filename);
                    printf("%s", log_message);
                    write_to_log(log_message, logs_dir);    
                    return -1;
                }

                // Если файл не закончился, просим LWS вызвать нас снова, как только сеть освободится
                if (!is_last) {
                    lws_callback_on_writable(wsi);
                } else {
                    sprintf(log_message, "%s Файл %s полностью передан в сеть! Ожидаем ответ 200 OK...\n", timestamp, upload->filename);
                    printf("%s", log_message);
                    write_to_log(log_message, logs_dir);
                }
            }
            break;

        case LWS_CALLBACK_ESTABLISHED_CLIENT_HTTP: {
            // Данный колбэк вызывается, когда заголовки POST-запроса успешно отправлены.
            // Запрашиваем первый вызов writable для отправки тела файла.
            lws_callback_on_writable(wsi);
            break;
        }

        case LWS_CALLBACK_RECEIVE_CLIENT_HTTP_READ:
            // Сюда придет ответ от сервера (например, тело ответа 200 OK)
            // Мы можем его вывести или проигнорировать
            break;

        case LWS_CALLBACK_RECEIVE_CLIENT_HTTP: {
            // Сервер прислал финальный статус (например, 200 OK)
            unsigned int status = lws_http_client_http_response(wsi);
            snprintf(log_message, LOG_MESSAGE_LEN, "%s Файл %s Сервер ответил HTTP статусом: %u.\n", timestamp, upload->filename, status);
            printf("%s", log_message);
            write_to_log(log_message, logs_dir);
            return -1; // Возвращаем -1, чтобы закрыть это HTTP-соединение, так как задача выполнена
        }

        case LWS_CALLBACK_CLIENT_HTTP_DROP_PROTOCOL:
            // Очистка ресурсов при закрытии соединения
            if (upload) {
                if (upload->fp) fclose(upload->fp);
                free(upload);
            }
            break;

        default:
            break;
    }
    return 0;
}

// Триггер callback-обработчика событий http-клиента
void start_http_upload(struct lws_context *context, const char *server_ip, int port, 
                       const char *upload_path, const char *local_filepath) {
    
    // 1. Открываем файл и замеряем его размер
    FILE *fp = fopen(local_filepath, "rb");
    if (!fp) {
        return;
    }
    
    fseek(fp, 0, SEEK_END);
    size_t file_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    // 2. Выделяем структуру для отслеживания прогресса
    client_file_upload_data *upload_status = malloc(sizeof(client_file_upload_data));
    upload_status->fp = fp;
    upload_status->total_size = file_size;
    upload_status->sent_size = 0;
    strcpy(upload_status->filename, local_filepath);

    // 3. Конфигурируем HTTP POST запрос
    struct lws_client_connect_info info = {0};
    
    info.context = context;
    info.address = server_ip;
    info.port = port;
    info.path = upload_path; // Передаем полученный с сервера путь, например "/upload_status/550e8400..."
    info.host = info.address;
    info.origin = info.address;
    info.method = "POST";
    info.protocol = "http_file_tranfer-protocol"; // Протокол LWS для HTTP-запросов (задан в массиве структур lws_protocols)
    info.userdata = upload_status; // Привязываем данные файла к wsi-сессии

    // 4. Инициируем асинхронное подключение
    struct lws *wsi = lws_client_connect_via_info(&info);
    if (!wsi) {
        // lwsl_err("Не удалось инициировать HTTP подключение к серверу\n");
        fclose(fp);
        free(upload_status);
    }
}

// Callback-обработчик событий вебсокет-клиента
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
            
            if (msg->type == FILE_UPLOAD_ACK) {
                char url[50];
                char filename[MAX_FILENAME_LEN];
                char *token = strtok(msg->text, "|");
                if (token != NULL) {
                    strcpy(url, token + 4);
                } else {         
                    return 0;
                }
                token = strtok(NULL, ":");
                if (token != NULL) {
                    strcpy(filename, token + strlen(token) + 1);
                } else {
                    return 0;
                }
                start_http_upload(lws_get_context(wsi), server_address, server_port, url, filename);
                break;       
            }
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
        .name = "http_file_tranfer-protocol",
        .callback = callback_http_client,
        .per_session_data_size = sizeof(http_client_session),
        .rx_buffer_size = 4096,                            
        .id = 0,                                           
        .user = NULL,
        .tx_packet_size = 0  
    },
    {
        .name = "chat-protocol",
        .callback = callback_chat_client,
        .per_session_data_size = 0,
        .rx_buffer_size = sizeof(message_format),
        .id = 1,
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
    int opt;

    while ((opt = getopt(argc, argv, "s:p:d:h")) != -1) {
        switch (opt) {
            case 's':
                strcpy(server_address, optarg);
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
    ccinfo.protocol = protocols[1].name; // "chat-protocol"
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
