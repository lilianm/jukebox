CC=gcc
LD=gcc

CFLAGS=-D_GNU_SOURCE -Wunused -W -Wall -Werror -g -I. -fPIC -std=gnu99
LD8FLAGS=--Wall -g -fPIC
LDFLAGS_HTTP=${LD_FLAGS} -lm
LDFLAGS_ENCODER=${LD_FLAGS} -lsqlite3 -pthread

all: httpd encoder

install: all

httpd: sck.o http.o
	${LD} -o $@ $+ ${LDFLAGS_HTTP}

encoder: encoder.o mp3.o thread_pool.o db.o mstring.o
	${LD} -o $@ $+ ${LDFLAGS_ENCODER} 
