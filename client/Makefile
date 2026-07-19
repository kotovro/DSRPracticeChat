CC = gcc
CFLAGS += -Wall -Wextra -Werror
CFLAGS += -O3
CFLAGS += -I../common
CFLAGS += -I../utils/
CFLAGS += -g3 
# CFLAGS += -O0
LDLIBS += -lpthread
LDLIBS += -lwebsockets 
BUILD_DIR ?= build

client: | build $(BUILD_DIR)/client.o $(BUILD_DIR)/utils.o
	$(CC) $(CFLAGS) -o client client.o utils.o $(LDLIBS) 

build:
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/client.o: client.c
	$(CC) $(CFLAGS) -c client.c

$(BUILD_DIR)/utils.o: ../utils/utils.c
	$(CC) $(CFLAGS) -c ../utils/utils.c -o utils.o


clean:
	rm -rf client *.o

