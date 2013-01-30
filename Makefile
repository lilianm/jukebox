çCC?=gcc
LD=gcc

CFLAGS=-D_GNU_SOURCE -W -Wall -Wextra -Werror -g -I. -fPIC -std=gnu99 -DDEBUG

LDFLAGS=-g -fPIC
LDFLAGS_JUKEBOX=${LDFLAGS} -lsqlite3 -lpthread

all: jukebox jukebox.so

install: all

jukebox.so: jukebox.o mp3.o mtimer.o encoder.o thread_pool.o db.o mstring.o display.o channel.o event.o http.o event_output.o base64.o hash.o user.o mempool.o http_tool.o
	${LD} --shared -o $@ $+ ${LDFLAGS_JUKEBOX}

jukebox: main.o
	${LD} -o $@ $+ jukebox.so
