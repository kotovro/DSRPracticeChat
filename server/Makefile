CC = gcc
CFLAGS += -Wall -Wextra -Werror
CFLAGS += -O3
LDLIBS = -lwebsockets

server: server.o
	$(CC) $(CFLAGS) -o server server.o $(LDLIBS)

server.o: server.c
	$(CC) $(CFLAGS) -c server.c





