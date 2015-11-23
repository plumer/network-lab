MYIP=$(shell /sbin/ifconfig | grep -Eo '(inet |eth ).+([0-9]*\.){3}[0-9]*' | grep -Eo '([0-9]*\.){3}[0-9]*' | grep -v '127.0.0.1'| grep -v '255.*')
MYPORT=10981
CC=gcc
CFLAGS= -std=gnu99 -g -pg -lpthread
LIBS= -lpthread
TARGET=simpletorrent
SOURCES=src/util.c \
		src/bencode.c \
		src/sha1.c \
		src/shutdown.c \
		src/make_tracker_request.c \
		src/parse_announce_url.c \
		src/parsetorretnfile.c \
		src/process_tracker_response.c \
		src/simpletorrent.c \
		src/pwp_daemon.c \
		src/fileio.c

OBJS=src/util.o \
	src/bencode.o \
	src/sha1.o\
	src/shutdown.o \
	src/make_tracker_request.o \
	src/parse_announce_url.o \
	src/parsetorrentfile.o \
	src/process_tracker_response.o \
	src/simpletorrent.o \
	src/pwp_daemon.o \
	src/fileio.o

all: ${TARGET}

${TARGET}: ${OBJS}
	${CC} ${CFLAGS} -o bin/${TARGET} ${LIBS} ${OBJS} -lpthread

%.o: $.c
	$(CC) -c $(CFLAGS) ${LIBS} $@ $<

clean:
	rm -rf src/*.o
	rm -rf bin/${TARGET}
	rm -rf src/*.core
	rm -rf *.o
	rm -rf ${TARGET}
	rm -rf *.core$

# 做种
sd:
	./bin/simpletorrent ./seed/novel.torrent ${MYIP} ./data/novel.txt 1

# 下载
down:
	./bin/simpletorrent ./seed/novel.torrent ${MYIP} ./download/novel.txt 0

# 下载电影
film:
	./bin/simpletorrent ./seed/test_movie.torrent ${MYIP} ./download 0

mulfile:
	./bin/simpletorrent ./seed/dir_test.torrent ${MYIP} ./download 0

# 服务器运行tracker
tracker:
	./tools/64bit/tracker -i ${MYIP} -p ${MYPORT}

# 服务器运行tracker.debug
trackerdebug:
	./tools/64bit/tracker.debug -i ${MYIP} -p ${MYPORT}

.PHONY: all clean
