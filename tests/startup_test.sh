#!/bin/bash

# Настройка строгого режима: падать при ошибках
set -euo pipefail


# Переменные путей
SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
ROOT_DIR=$(cd "$SCRIPT_DIR/.." && pwd)
SERVER_BIN="$ROOT_DIR/server/server"
CLIENT_BIN="$ROOT_DIR/client/client"

# 1. Создаем уникальную временную папку для изоляции окружения
TMP_DIR=$(mktemp -d)
echo $TMP_DIR
SERVER_LOG="$TMP_DIR/server.log"
CLIENT_LOG="$TMP_DIR/client.log"

# Именованные каналы для отправки команд в stdin
SERVER_IN="$TMP_DIR/server.fifo"
CLIENT_IN="$TMP_DIR/client.fifo"

# 2. Создаем FIFO-каналы
mkfifo "$SERVER_IN"
mkfifo "$CLIENT_IN"

# 3. Настраиваем автоматическую очистку при любом исходе теста
cleanup() {
    echo "Останавливаем процессы и удаляем временные файлы..."
    # Закрываем удерживающие дескрипторы (чтобы фоновые процессы завершились)
    exec 3>&- 2>/dev/null || true
    exec 4>&- 2>/dev/null || true
    # Принудительно гасим сервер и клиент, если они еще живы
    kill $(jobs -p) 2>/dev/null || true
    # Удаляем всю временную папку вместе с FIFO
    rm -rf "$TMP_DIR"
}
trap cleanup EXIT


echo "=== Тест: запуск сервера и клиента ==="
echo "=== Запуск инфраструктуры теста ==="

# 4. Запускаем СЕРВЕР в фоне. Его stdin читает из FIFO, логи пишутся в файл.
stdbuf -oL "$SERVER_BIN" < "$SERVER_IN" > "$SERVER_LOG" 2>&1 &
sleep 10.5 # Даем время на инициализацию портов

# 5. Запускаем КЛИЕНТ в фоне аналогичным образом.
stdbuf -oL "$CLIENT_BIN" < "$CLIENT_IN" > "$CLIENT_LOG" 2>&1 &
sleep 10.5

# 6. Открываем FIFO на запись и вешаем на свободные дескрипторы (3 и 4).
# Без этого фоновые процессы сразу закроются, решив, что поток ввода завершен (EOF).
exec 3> "$SERVER_IN"
exec 4> "$CLIENT_IN"

# Вспомогательная функция для проверки логов на наличие нужного текста
assert_contains() {
    local file=$1
    local expected_text=$2
    local timeout=3
    local count=0

    # Ждем появления текста (учитываем задержки буферизации приложений)
    while ! grep -qF "$expected_text" "$file"; do
        sleep 0.2
        count=$((count + 1))
        if [ "$count" -gt $((timeout * 5)) ]; then
            echo "[ FAIL ] Текст '$expected_text' не найден в $file!"
            echo "--- ПОСЛЕДНИЕ СТРОКИ ЛОГА ---"
            tail -n 10 "$file"
            exit 1
        fi
    done
}

# =====================================================================
# СЦЕНАРИЙ ТЕСТА
# =====================================================================
echo "=== Начало выполнения сценария ==="

# Шаг 1: Проверяем, что сервер успешно запустился и ждет подключений
echo "[ STEP 1 ] Запускаем сервер."
assert_contains "$SERVER_LOG" "Сервер чата запущен на порту 8080"


# Шаг 2: Проверяем, что клиент подключился к серверу
echo "[ STEP 2 ] Проверяем, что клиент запустился и подключился к серверу."
assert_contains "$CLIENT_LOG" "Подключено!"
assert_contains "$SERVER_LOG" "Новый клиент подключился. Всего клиентов: 1"

# Шаг 3: Имитируем ввод пользователя в КЛИЕНТЕ (отправка команды авторизации)
# Пишем в дескриптор 4, который привязан к клиентскому FIFO
echo "[ STEP 3 ] Отправлена команда /login от клиента."
echo "/login user2" >&4
sleep 0.5

# Шаг 4: Проверяем, что клиент авторизовался
echo "[ STEP 4 ] Проверка авторизации."
assert_contains "$SERVER_LOG" "Клиент сменил имя на: user2"
assert_contains "$CLIENT_LOG" "Добро пожаловать в чат, user2!"

# Шаг 5: Имитируем ввод пользователя на клиенте (рассылка сообщения в общий чат)
echo "[ STEP 5 ] Отправка cообщения в общий чат от клиента."
echo "Hello Everyone!" >&4
sleep 0.5
assert_contains "$SERVER_LOG" "{} [user2] для [Общий чат]: Hello Everyone!"
assert_contains "$SERVER_LOG" "Рассылаем сообщение от user2 всем клиентам (кроме никого)"
sleep 0.5

# Шаг 6: Проверяем, получил ли КЛИЕНТ сообщение от сервера
echo "[ STEP 6 ] Получено сообщение в общий чат."
assert_contains "$CLIENT_LOG" "[user2] для [Общий чат]: Hello Everyone!"


echo "=== [ SUCCESS ] Тест на подключение клиента к серверу и отправку сообщения в общий чат пройден! ==="

