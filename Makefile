CC=gcc
CFLAGS=-g
CPPFLAGS=-Wall -pedantic

.PHONY: all clean

all: server

# server
server: server.o
	$(CC) -o server server.o $(CFLAGS)
server.o: server.c
	$(CC) -o server.o -c $(CFLAGS) $(CPPFLAGS) server.c

# clean
clean:
	rm -rf *.o server

