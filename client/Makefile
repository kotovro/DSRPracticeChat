CC = gcc
CFLAGS += -Wall -Wextra -Werror
CFLAGS += -O3
CFLAGS += -I../common
CFLAGS += -I../utils/
LDLIBS += -lpthread
LDLIBS += -lwebsockets 

client: client.o utils.o
	$(CC) $(CFLAGS) -o client client.o utils.o $(LDLIBS)

client.o: client.c
	$(CC) $(CFLAGS) -c client.c

utils.o: ../utils/utils.c
	$(CC) $(CFLAGS) -c ../utils/utils.c -o utils.o


clean:
	rm -rf client *.o

