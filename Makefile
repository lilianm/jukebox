CC?=gcc
LD=gcc

CFLAGS=-D_GNU_SOURCE -W -Wall -Wextra -Wno-self-assign -Werror -g -I. -fPIC -std=gnu99 ${shell pkg-config --cflags ruby-1.9} -DDEBUG

LDFLAGS=-g -fPIC
LDFLAGS_JUKEBOX=${LDFLAGS} -lsqlite3 -lpthread
LDFLAGS_RUBY=${LDFLAGS} --shared ${shell pkg-config --libs ruby-1.9}
LDFLAGS_HTTP=${LDFLAGS} -lm

all: httpd  jukebox # jukebox_fw.so

install: all

httpd: sck.o http.o
	${LD} -o $@ $+ ${LDFLAGS_HTTP}

jukebox: jukebox.o sck.o mp3.o mtimer.o encoder.o thread_pool.o db.o mstring.o display.o channel.o event.o
	${LD} -o $@ $+ ${LDFLAGS_JUKEBOX}

jukebox_fw.so: display.o jukebox_fw.o mstring.o mp3.o
	${LD} -o $@ $+ ${LDFLAGS_RUBY}
