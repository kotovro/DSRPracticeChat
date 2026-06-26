CC = gcc
CFLAGS += -Wall -Wextra -Werror
CFLAGS += -O3
LDLIBS += -lwebsockets 
LDLIBS += -lpthread

client: client.o
	$(CC) $(CFLAGS) -o client client.o $(LDLIBS)

client.o: client.c
	$(CC) $(CFLAGS) -c client.c





