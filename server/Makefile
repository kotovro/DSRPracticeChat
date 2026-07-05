CC = gcc
CFLAGS += -Wall -Wextra -Werror
CFLAGS += -O3
CFLAGS += -I../common
# CFLAGS += -I../utils/cJSON
CFLAGS += -I../utils/
LDLIBS = -lwebsockets

server: server.o client_login.o utils.o parser.o server_utils.o
	$(CC) $(CFLAGS) -o server  server.o client_login.o utils.o parser.o server_utils.o $(LDLIBS)

server.o: server.c
	$(CC) $(CFLAGS) -c server.c

parser.o: parser.c
	$(CC) $(CFLAGS) -c parser.c

client_login.o: client_login.c
	$(CC) $(CFLAGS) -c client_login.c

utils.o: ../utils/utils.c
	$(CC) $(CFLAGS) -c ../utils/utils.c -o utils.o

server_utils.o: ../utils/server_utils.c
	$(CC) $(CFLAGS) -c ../utils/server_utils.c -o server_utils.o

.PHONY: clean

clean:
	rm -rf server *.o
# cJSON.o: ../utils/cJSON/cJSON.c
# 	$(CC) $(CFLAGS) -c ../utils/cJSON/cJSON.c -o cJSON.o