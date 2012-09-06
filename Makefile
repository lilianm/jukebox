CC=gcc
LD=gcc

CFLAGS=-D_GNU_SOURCE -Wunused -W -Wall -Werror -g -I. -fPIC -std=gnu99
LDFLAGS=-lm -Wall -g -fPIC

all: httpd

install: all

httpd: sck.o http.o
	${LD}  -o $@ $+ ${LDFLAGS}
