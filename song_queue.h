#ifndef __SONG_QUEUE_H__
#define __SONG_QUEUE_H__

typedef struct song_queue song_queue_t;

void song_queue_init(song_queue_t *q, size_t size);

song_queue_t * song_queue_new(size_t size);

void song_queue_clean(song_queue_t *q);

void song_queue_free(song_queue_t *q);

int song_queue_get(song_queue_t *q, int idx);

void song_queue_add(song_queue_t *q, int mid);

int song_queue_next(song_queue_t *q);

int song_queue_previous(song_queue_t *q);

void song_queue_shuffle(song_queue_t *q);

#endif /* __SONG_QUEUE_H__ */
