CC = gcc
CFLAGS += -Wall -Wextra -Werror
CFLAGS += -O3
LDLIBS = -lwebsockets

server: server.o client_login.o
	$(CC) $(CFLAGS) -o server server.o client_login.o $(LDLIBS)

server.o: server.c
	$(CC) $(CFLAGS) -c server.c

client_login.o: client_login.c
	$(CC) $(CFLAGS) -c client_login.c

