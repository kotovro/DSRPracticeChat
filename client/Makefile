CC = gcc
CFLAGS += -Wall -Wextra -Werror
CFLAGS += -O3
CFLAGS += -I../common
CFLAGS += -I../utils/
CFLAGS += -D_GNU_SOURCE
LDLIBS += -lpthread
LDLIBS += -lwebsockets

BUILD_DIR ?= build

# Debug: make DEBUG=1 добавит debug-символы и отключит оптимизацию
DEBUG ?= 0
ifeq ($(DEBUG),1)
    CFLAGS += -g3 -O0
endif

OBJS = $(BUILD_DIR)/client.o $(BUILD_DIR)/utils.o

client: $(OBJS)
	$(CC) $(CFLAGS) -o client $(OBJS) $(LDLIBS)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/client.o: client.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c client.c -o $@

$(BUILD_DIR)/utils.o: ../utils/utils.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c ../utils/utils.c -o $@

clean:
	rm -rf client $(BUILD_DIR)

.PHONY: clean