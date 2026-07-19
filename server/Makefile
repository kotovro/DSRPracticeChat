CC = gcc
CFLAGS += -Wall -Wextra -Werror -Wpedantic
CFLAGS += -O3
CFLAGS += -I../common
# CFLAGS += -I../utils/cJSON
CFLAGS += -I../utils/
LDLIBS = -lwebsockets
LDLIBS += -lpthread
BUILD_DIR ?= build

server: | build server build/server.o build/client_login.o build/utils.o build/parser.o build/server_utils.o build/user_storage.o build/group_storage.o build/message_storage.o build/file_storage.o
	$(CC) $(CFLAGS) -o server \
		build/server.o \
		build/client_login.o \
		build/utils.o \
		build/parser.o \
		build/server_utils.o \
		build/user_storage.o \
		build/group_storage.o \
		build/message_storage.o \
		build/file_storage.o \
		$(LDLIBS)

build:
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/server.o: server.c
	$(CC) $(CFLAGS) -c server.c -o build/server.o

$(BUILD_DIR)/parser.o: parser.c
	$(CC) $(CFLAGS) -c parser.c -o build/parser.o

$(BUILD_DIR)/client_login.o: client_login.c
	$(CC) $(CFLAGS) -c client_login.c -o build/client_login.o

$(BUILD_DIR)/file_storage.o: file_storage.c
	$(CC) $(CFLAGS) -c file_storage.c -o build/file_storage.o

$(BUILD_DIR)/user_storage.o: user_storage.c
	$(CC) $(CFLAGS) -c user_storage.c -o build/user_storage.o

$(BUILD_DIR)/group_storage.o: group_storage.c
	$(CC) $(CFLAGS) -c group_storage.c -o build/group_storage.o

$(BUILD_DIR)/message_storage.o: message_storage.c
	$(CC) $(CFLAGS) -c message_storage.c -o build/message_storage.o

$(BUILD_DIR)/utils.o: ../utils/utils.c
	$(CC) $(CFLAGS) -c ../utils/utils.c -o build/utils.o

$(BUILD_DIR)/server_utils.o: ../utils/server_utils.c
	$(CC) $(CFLAGS) -c ../utils/server_utils.c -o build/server_utils.o


.PHONY: clean

clean:
	rm -rf build server
# cJSON.o: ../utils/cJSON/cJSON.c
# 	$(CC) $(CFLAGS) -c ../utils/cJSON/cJSON.c -o cJSON.o