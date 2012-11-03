CC=gcc
LD=gcc

CFLAGS=-D_GNU_SOURCE -Wunused -W -Wall -Werror -g -I. -fPIC -std=gnu99 ${shell pkg-config --cflags ruby-1.9}

LDFLAGS=-Wall -g -fPIC
LDFLAGS_ENCODER=${LD_FLAGS} -lsqlite3 -pthread
LDFLAGS_RUBY=${LD_FLAGS} --shared ${shell pkg-config --libs ruby-1.9}

all: encoder jukebox_fw.so

install: all

encoder: encoder.o mp3.o thread_pool.o db.o mstring.o
	${LD} -o $@ $+ ${LDFLAGS_ENCODER}

jukebox_fw.so: display.o jukebox_fw.o mstring.o mp3.o mp3stream.o
	${LD} -o $@ $+ ${LDFLAGS_RUBY}
