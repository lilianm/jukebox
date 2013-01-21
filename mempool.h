#ifndef __MEMPOOL_H__
#define __MEMPOOL_H__

#include <stdlib.h>

typedef struct mempool mempool_t;

mempool_t * mempool_new(size_t object_size, size_t chunk_size);

void mempool_delete(mempool_t *mempool);

void * mempool_alloc(mempool_t *mempool);

void mempool_free(mempool_t *mempool, void *d);

#endif /* __MEMPOOL_H__ */
