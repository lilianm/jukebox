#include <stdint.h>

#include "mempool.h"

typedef struct list {
    struct list *next;
} list_t;

struct mempool {
    size_t  object_size;
    size_t  chunk_size;

    list_t *free_list;
    list_t *chunk;
};

mempool_t * mempool_new(size_t object_size, size_t chunk_size)
{
    mempool_t *pool;

    if(object_size < sizeof(list_t)) {
        object_size = sizeof(list_t);
    }

    pool = (mempool_t*) malloc(sizeof(mempool_t));
    pool->object_size       = object_size;
    pool->chunk_size        = chunk_size;
    pool->free_list         = NULL;
    pool->chunk             = NULL;

    return pool;    
}

void mempool_delete(mempool_t *pool)
{
    list_t *cur;
    list_t *next;

    cur = pool->chunk;

    while(cur) {
        next = cur->next;
        free(cur);
        cur = next;
    }
}

void mempool_alloc_chunk(mempool_t *pool)
{
    size_t  size;
    list_t *list;
    int     i;

    size  = pool->object_size * pool->chunk_size;
    size += sizeof(list_t);

    list = (list_t*) malloc(size);

    list->next  = pool->chunk;
    pool->chunk = list;

    list++;

    for(i = 0; i < pool->chunk_size; ++i) {
        list->next      = pool->free_list;
        pool->free_list = list;

        list = (list_t *)((uint8_t *)list + pool->object_size);
    }
    
}

void * mempool_alloc(mempool_t *pool)
{
    void *d;

    if(pool->free_list == NULL)
        mempool_alloc_chunk(pool);

    d = pool->free_list;

    pool->free_list = pool->free_list->next;

    return d;
}

void mempool_free(mempool_t *pool, void *d)
{
    list_t *list;

    list = (list_t *) d;

    list->next      = pool->free_list;
    pool->free_list = list;
}

#ifdef TEST

#include <stdio.h>

#define CHECK_EQ(test, value)                                           \
({                                                                      \
    nb_test++;                                                          \
    if((test) == (value))                                               \
        nb_success ++;                                                  \
    else                                                                \
        fprintf(stderr, "Line %i Need return " #value " for test " #test "\n",  __LINE__); \
})

#define CHECK_NEQ(test, value)                                          \
({                                                                      \
    nb_test++;                                                          \
    if((test) != (value))                                               \
        nb_success ++;                                                  \
    else                                                                \
        fprintf(stderr, "Line %i Need return " #value " for test " #test "\n",  __LINE__); \
})

int main(void)
{
    mempool_t *pool;
    void      *e[4];
    int        nb_test    = 0;
    int        nb_success = 0;
    
    CHECK_NEQ(pool = mempool_new(16, 2), NULL);
    CHECK_NEQ(e[0] = mempool_alloc(pool), NULL);
    CHECK_NEQ(e[1] = mempool_alloc(pool), NULL);
    CHECK_NEQ(e[2] = mempool_alloc(pool), NULL);
    mempool_free(pool, e[1]);
    mempool_delete(pool);

    fprintf(stderr, "Nb test success %i/%i\n", nb_success, nb_test);

    if(nb_success != nb_test)
        return 1;
    return 0;
}

#endif /* TEST */
