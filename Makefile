CC = gcc
CFLAGS = -Wall -g -c
LDFLAGS = -lnsl

.PHONY: all clean

all: rpc-server rpc-client

rpc-server: server.a rpc.h
	$(CC) server.a -o rpc-server $(LDFLAGS)

rpc-client: client.a rpc.h
	$(CC) client.a -o rpc-client $(LDFLAGS)

rpc.o: rpc.c rpc.h
	$(CC) $(CFLAGS) rpc.c -o rpc.o

server.o: server.c rpc.h
	$(CC) $(CFLAGS) server.c -o server.o

client.o: client.c rpc.h
	$(CC) $(CFLAGS) client.c -o client.o

server.a: server.o rpc.o
	ar rcs server.a server.o rpc.o

client.a: client.o rpc.o
	ar rcs client.a client.o rpc.o


clean:
	rm -f rpc-server rpc-client *.o *.a
