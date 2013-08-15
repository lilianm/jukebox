#ifndef __DB_H__
#define __DB_H__

#include "mp3.h"
#include "song.h"

int db_init(void);

void db_song_save(song_t *song);

song_t * db_song_load(int mid);

int db_song_random(void);

typedef void (*scan_fn)(const unsigned char *src, time_t mtime, void *data);

void db_scan_song(scan_fn fn, void *data);

#endif /* __DB_H__ */
