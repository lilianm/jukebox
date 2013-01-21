#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "hash.h"

struct hash_entry {
    hash_entry_t      *next;
    void              *key;
    void              *data;
    uint32_t           hash;
};

static inline void hash_entry_init(hash_t *h)
{
    unsigned int i;

    memset(h->entry, 0,
           (h->size + (h->size >> 2)) * sizeof(hash_entry_t));
    for(i = 0; i < (h->size >> 2) - 1; ++i) {
        h->entry[h->size + i].next = &h->entry[h->size + i + 1];
    }
    h->free_list = &h->entry[h->size];
}

void hash_init(hash_t *h, cmp_f cmp, hash_f hash, size_t size)
{
    unsigned int nb;

    assert(cmp);
    assert(hash);
    assert(size > 1 && ((size & (size - 1)) == 0)); // Check size is n^2

    nb           = size + (size >> 2); // Alloc 25% overhead

    h->entry     = (hash_entry_t *) malloc(nb * sizeof(hash_entry_t));
    h->size      = size;
    h->nb_entry  = 0;
    h->cmp_fn    = cmp;
    h->hash_fn   = hash;

    hash_entry_init(h);
}

hash_t * hash_new(cmp_f cmp, hash_f hash, size_t size)
{
    hash_t *h;

    h = (hash_t*) malloc(sizeof(hash_t));

    hash_init(h, cmp, hash, size);

    return h;
}

void * hash_get(hash_t *h, void *key)
{
    uint32_t      hash;
    size_t        mask;
    hash_entry_t *entry;

    hash = h->hash_fn(key);
    mask = h->size - 1;

    entry = &h->entry[hash & mask];

    if(entry->key == NULL)
        return NULL;

    while(entry) {
        if(entry->hash == hash &&
           h->cmp_fn(key, entry->key))
            break;
        entry = entry->next;
    }

    if(entry == NULL)
        return NULL;

    return entry->data;
}

static inline void hash_extend(hash_t *h)
{
    hash_entry_t *nentry;
    hash_entry_t *tentry;
    hash_entry_t *entry;
    unsigned int  nb;

    hash_entry_t *oentry;
    size_t        osize;

    size_t        i;
    size_t        mask;

    oentry    = h->entry;
    osize     = h->size;
    h->size <<= 1;
    mask      = h->size - 1;
    nb        = h->size + (h->size >> 2); // Alloc 25% overhead

    entry     = (hash_entry_t *) malloc(nb * sizeof(hash_entry_t));
    h->entry  = entry;

    hash_entry_init(h);

    for(i = 0; i < osize; ++i) {
        entry = &oentry[i];
        if(entry->key == NULL)
            continue;

        while(entry) {
            nentry = &h->entry[entry->hash & mask];

            if(nentry->key == NULL) {
                nentry->data  = entry->data;
                nentry->key   = entry->key;
                nentry->hash  = entry->hash;
                nentry->next  = NULL;
            } else {
                tentry       = h->free_list;
                h->free_list = h->free_list->next;
                tentry->next = nentry->next;
                nentry->next = tentry;
                nentry       = tentry;
                nentry->data = entry->data;
                nentry->key  = entry->key;
                nentry->hash = entry->hash;
            }

            entry = entry->next;
        }
    }
    
    free(oentry);
}

int hash_add(hash_t *h, void *key, void *d)
{
    uint32_t       hash;
    size_t         mask;
    hash_entry_t  *entry;
    hash_entry_t **nentry;

    // Hash load >= 75%
    if(h->nb_entry >= ((h->size >> 1) + (h->size >> 2)))
        hash_extend(h);

    hash = h->hash_fn(key);
    mask = h->size - 1;

    entry = &h->entry[hash & mask];

    if(entry->key == NULL) {
        entry->key  = key;
        entry->data = d;
        entry->hash = hash;

        h->nb_entry++;
        return 0;
    }

    nentry = &entry->next;
    while(*nentry) {
        entry = *nentry;
        if(entry->hash == hash &&
           h->cmp_fn(key, (entry->key)))
            break;
        nentry = &entry->next;
    }

    entry = *nentry;

    if(entry) {
        entry->data = d;

        return 0;
    }

    if(h->free_list == NULL) {
        hash_extend(h);
        return hash_add(h, key, d);
    }

    entry        = h->free_list;
    h->free_list = h->free_list->next;

    entry->key   = key;
    entry->data  = d;
    entry->hash  = hash;
    entry->next  = NULL;

    *nentry      = entry;

    h->nb_entry++;

    return 0;
}

void * hash_remove(hash_t *h, void *key)
{
    uint32_t       hash;
    size_t         mask;
    hash_entry_t  *entry;
    hash_entry_t  *old_entry;
    hash_entry_t **nentry;
    void          *d;

    hash = h->hash_fn(key);
    mask = h->size - 1;

    entry = &h->entry[hash & mask];

    if(entry->key == NULL)
        return NULL;

    nentry = NULL;
    while(entry) {
        if(entry->hash == hash &&
           h->cmp_fn(key, (entry->key)))
            break;
        nentry = &entry->next;
        entry  = entry->next;
    }

    if(entry == NULL)
        return NULL;

    if(nentry == NULL) {
        d = entry->data;

        if(entry->next) {
            old_entry       = entry->next;
            *entry          = *entry->next;
            old_entry->next = h->free_list;
            h->free_list    = old_entry;
        } else {
            entry->data = NULL;
            entry->key  = NULL;
        }

        h->nb_entry--;
        return d;
    }

    d            = entry->data;
    *nentry      = entry->next;
    entry->next  = h->free_list;
    h->free_list = entry;

    h->nb_entry--;

    return d;
}
