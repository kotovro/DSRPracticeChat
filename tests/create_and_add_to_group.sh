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
CLIENT2_LOG="$TMP_DIR/client2.log"

# Именованные каналы для отправки команд в stdin
SERVER_IN="$TMP_DIR/server.fifo"
CLIENT_IN="$TMP_DIR/client.fifo"
CLIENT2_IN="$TMP_DIR/client2.fifo"

# 2. Создаем FIFO-каналы
mkfifo "$SERVER_IN"
mkfifo "$CLIENT_IN"
mkfifo "$CLIENT2_IN"

# 3. Настраиваем автоматическую очистку при любом исходе теста
cleanup() {
    echo "Останавливаем процессы и удаляем временные файлы..."
    # Закрываем удерживающие дескрипторы (чтобы фоновые процессы завершились)
    exec 3>&- 2>/dev/null || true
    exec 4>&- 2>/dev/null || true
    exec 5>&- 2>/dev/null || true
    # Принудительно гасим сервер и клиент, если они еще живы
    kill $(jobs -p) 2>/dev/null || true
    # Удаляем всю временную папку вместе с FIFO
    rm -rf "$TMP_DIR"
}
trap cleanup EXIT


echo "=== Тест: создание группы, отправка в неё соообщений и добалвние пользователя в группу ==="
echo "=== Запуск инфраструктуры теста ==="

# 4. Запускаем СЕРВЕР в фоне. Его stdin читает из FIFO, логи пишутся в файл.
stdbuf -oL "$SERVER_BIN" < "$SERVER_IN" > "$SERVER_LOG" 2>&1 &
sleep 10.5 # Даем время на инициализацию портов

# 5. Запускаем КЛИЕНТ в фоне аналогичным образом.
stdbuf -oL "$CLIENT_BIN" < "$CLIENT_IN" > "$CLIENT_LOG" 2>&1 &
sleep 10.5

# 5. Запускаем второго КЛИЕНТА в фоне аналогичным образом.
stdbuf -oL "$CLIENT_BIN" < "$CLIENT2_IN" > "$CLIENT2_LOG" 2>&1 &
sleep 10.5

# 6. Открываем FIFO на запись и вешаем на свободные дескрипторы (3 и 4).
# Без этого фоновые процессы сразу закроются, решив, что поток ввода завершен (EOF).
exec 3> "$SERVER_IN"
exec 4> "$CLIENT_IN"
exec 5> "$CLIENT2_IN"

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

assert_not_contains() {
    local file=$1
    local forbidden_text=$2
    local timeout=2      # Сколько секунд мы готовы ждать/наблюдать за логом
    local interval=0.2   # Интервал проверки лога
    local count=0

    # Вычисляем максимальное количество итераций цикла
    # Например, 2 секунды / 0.2 секунды = 10 проверок
    local max_iterations=$(echo "$timeout / $interval" | bc 2>/dev/null || echo $((timeout * 5)))

    while [ "$count" -lt "$max_iterations" ]; do
        # Если текст ВСЁ-ТАКИ НАЙДЕН (-qF), то это провал теста
        if grep -qF "$forbidden_text" "$file"; then
            echo "[ FAIL ] Обнаружен запрещенный текст '$forbidden_text' в файле $file!"
            echo "--- СТРОКА С ОШИБКОЙ ИЗ ЛОГА ---"
            grep -nF "$forbidden_text" "$file" # Показываем строку и её номер
            echo "--------------------------------"
            exit 1
        fi
        
        sleep "$interval"
        count=$((count + 1))
    done

    # Если цикл завершился и текст так и не появился — тест успешно пройден
    echo "[  OK  ] Проверка пройдена: текст '$forbidden_text' отсутствует в $file."
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
assert_contains "$SERVER_LOG" "Новый клиент подключился. Всего клиентов: 2"

# Шаг 3: Jтправка команды авторизации клиентами
# Пишем в дескриптор 4, который привязан к клиентскому FIFO
echo "[ STEP 3 ] Отправлена команда /login от клиента."
echo "/login user1" >&4
sleep 1.5
echo "/login user2" >&5
sleep 1.5

# Шаг 4: Создание группы
echo "[ STEP 4 ] Отправка команды о создании новой группы."
echo "/add_group test_group" >&5
sleep 1.5
assert_contains "$SERVER_LOG" "Сервер пользователю user2 : Группа test_group успешно создана"
assert_contains "$CLIENT2_LOG" "[Сервер] для [test_group]: Группа test_group успешно создана"

# Шаг 5: Проверка того, что сообщения в группу получают только её участники
echo "[ STEP 5 ] Отправка сообщения в группу."
echo "[test_group] teto!" >&5
sleep 1.5
assert_contains "$SERVER_LOG" "Получено сообщение {} [user2] для [test_group]: teto!"
assert_not_contains "$CLIENT_LOG" "[user2] для [test_group]: teto!"
assert_contains "$CLIENT2_LOG" "[user2] для [test_group]: teto!"

# Шаг 5: Проверка того, что сообщения в группу получают только её участники
echo "[ STEP 5 ] Отправка сообщения в группу."
echo "[test_group] teto!" >&5
sleep 0.5
assert_contains "$SERVER_LOG" "Получено сообщение {} [user2] для [test_group]: teto!"
assert_not_contains "$CLIENT_LOG" "[user2] для [test_group]: teto!"
assert_contains "$CLIENT2_LOG" "[user2] для [test_group]: teto!"

# Шаг 6: Проверка добавления пользователя в группу
echo "[ STEP 6 ] Добавление пользователя в группу."
echo "[test_group] /add_user user1" >&5
sleep 0.5
assert_contains "$SERVER_LOG" "пользователю user2 : Пользователь user1 добавлен в группу"
assert_contains "$SERVER_LOG" "пользователю user1 : Пользователь user1 добавлен в группу"

# Шаг 7: Проверка добавления пользователя в группу
echo "[ STEP 7 ] Отправка и прием сообщений в группу."
echo "[test_group] hi!" >&4
sleep 0.5
assert_contains "$SERVER_LOG" "[user1] для [test_group]: hi!"
assert_contains "$CLIENT_LOG" "[user1] для [test_group]: hi!"
assert_contains "$CLIENT2_LOG" "[user1] для [test_group]: hi!"
sleep 10.5

echo "=== [ SUCCESS ] Тест на создание групп и отправку сообщений в них пройден! ==="

