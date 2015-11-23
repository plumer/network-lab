CFLAGS = -g

all: server.out client.out protocol.html

.PHONY: all

server.out: server.o database.o
	cc -o server.out server.o database.o -lpthread
	ctags -R
client.out: client.o
	cc -o client.out client.o -lncurses -lpthread

server.o: server.c database.c
	cc -c server.c database.c $(CFLAGS)
client.o: client.c
	cc -c client.c $(CFLAGS)

protocol.html: protocol_v1.md
	markdown protocol_v1.md > protocol_v1.html

clean:
	rm server.out client.out server.o client.o protocol_v1.html
