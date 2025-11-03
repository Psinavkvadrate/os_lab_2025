CC = gcc
CFLAGS = -I. -pthread -Wall -Wextra -O2

all: libcommon.a client server

libcommon.a: common.o
	ar rcs libcommon.a common.o

common.o: common.c common.h
	$(CC) $(CFLAGS) -c common.c -o common.o

client: client.o libcommon.a
	$(CC) client.o -o client libcommon.a $(CFLAGS)

server: server.o libcommon.a
	$(CC) server.o -o server libcommon.a $(CFLAGS)

client.o: client.c
	$(CC) $(CFLAGS) -c client.c -o client.o

server.o: server.c
	$(CC) $(CFLAGS) -c server.c -o server.o

clean:
	rm -f *.o *.a client server
