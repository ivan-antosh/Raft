CC=gcc
CFLAGS=-g
CPPFLAGS=-Wall -pedantic
AR=ar

LIBLIST_DIR=./list_lib

.PHONY: all clean

all: server proxy

# server
server: server.o libappendentries.a librequestvote.a libhelper.a $(LIBLIST_DIR)/liblist.a
	$(CC) -o server server.o $(CFLAGS) -L. -lappendentries -lrequestvote -lhelper -L$(LIBLIST_DIR) -llist
server.o: server.c types.h
	$(CC) -o server.o -c $(CFLAGS) $(CPPFLAGS) server.c -I. -I$(LIBLIST_DIR)

# proxy
proxy: proxy.o
	$(CC) -o proxy proxy.o $(CFLAGS)
proxy.o: proxy.c types.h
	$(CC) -o proxy.o -c $(CFLAGS) $(CPPFLAGS) proxy.c -I. -I$(LIBLIST_DIR)

# appendentries
libappendentries.a: append_entries.o
	$(AR) -rcs libappendentries.a append_entries.o
append_entries.o: append_entries.c append_entries.h types.h
	$(CC) -o append_entries.o -c $(CFLAGS) $(CPPFLAGS) append_entries.c -I.

# requestvote
librequestvote.a: request_vote.o
	$(AR) -rcs librequestvote.a request_vote.o
request_vote.o: request_vote.c request_vote.h
	$(CC) -o request_vote.o -c $(CFLAGS) $(CPPFLAGS) request_vote.c -I.

# helper
libhelper.a: helper.o
	$(AR) -rcs libhelper.a helper.o
helper.o: helper.c helper.h types.h
	$(CC) -o helper.o -c $(CFLAGS) $(CPPFLAGS) helper.c -I.

# liblist
$(LIBLIST_DIR)/liblist.a: $(LIBLIST_DIR)/list.o $(LIBLIST_DIR)/list_adders.o $(LIBLIST_DIR)/list_movers.o $(LIBLIST_DIR)/list_removers.o
	$(AR) -rcs $(LIBLIST_DIR)/liblist.a $(LIBLIST_DIR)/list.o $(LIBLIST_DIR)/list_adders.o $(LIBLIST_DIR)/list_movers.o $(LIBLIST_DIR)/list_removers.o

$(LIBLIST_DIR)/list.o: $(LIBLIST_DIR)/list.c $(LIBLIST_DIR)/list.h
	$(CC) -o $(LIBLIST_DIR)/list.o -c $(CFLAGS) $(CPPFLAGS) $(LIBLIST_DIR)/list.c -I$(LIBLIST_DIR)

$(LIBLIST_DIR)/list_adders.o: $(LIBLIST_DIR)/list_adders.c $(LIBLIST_DIR)/list.h
	$(CC) -o $(LIBLIST_DIR)/list_adders.o -c $(CFLAGS) $(CPPFLAGS) $(LIBLIST_DIR)/list_adders.c -I$(LIBLIST_DIR)

$(LIBLIST_DIR)/list_movers.o: $(LIBLIST_DIR)/list_movers.c $(LIBLIST_DIR)/list.h
	$(CC) -o $(LIBLIST_DIR)/list_movers.o -c $(CFLAGS) $(CPPFLAGS) $(LIBLIST_DIR)/list_movers.c -I$(LIBLIST_DIR)

$(LIBLIST_DIR)/list_removers.o: $(LIBLIST_DIR)/list_removers.c $(LIBLIST_DIR)/list.h
	$(CC) -o $(LIBLIST_DIR)/list_removers.o -c $(CFLAGS) $(CPPFLAGS) $(LIBLIST_DIR)/list_removers.c -I$(LIBLIST_DIR)

# clean
clean:
	rm -rf *.o *.a $(LIBLIST_DIR)/*.o $(LIBLIST_DIR)/*.a server

