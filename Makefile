CC=gcc
CFLAGS=-g
CPPFLAGS=-Wall -pedantic
AR=ar

.PHONY: all clean

all: server

# server
server: server.o libappendentries.a librequestvote.a
	$(CC) -o server server.o $(CFLAGS) -L. -lappendentries -lrequestvote
server.o: server.c
	$(CC) -o server.o -c $(CFLAGS) $(CPPFLAGS) server.c -I.

# appendentries
libappendentries.a: append_entries.o
	$(AR) -rcs libappendentries.a append_entries.o
append_entries.o: append_entries.c append_entries.h
	$(CC) -o append_entries.o -c $(CFLAGS) $(CPPFLAGS) append_entries.c -I.

# requestvote
librequestvote.a: request_vote.o
	$(AR) -rcs librequestvote.a request_vote.o
request_vote.o: request_vote.c request_vote.h
	$(CC) -o request_vote.o -c $(CFLAGS) $(CPPFLAGS) request_vote.c -I.

# clean
clean:
	rm -rf *.o *.a server

