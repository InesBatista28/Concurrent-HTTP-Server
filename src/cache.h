#ifndef CACHE_H
#define CACHE_H // I use include guards to prevent multiple inclusion of this header file.

#include <stddef.h> // I need size_t from here.

// This structure represents a single cache entry.
// I'm designing it to work in both a doubly-linked list (for LRU ordering)
// and a hash table chain (for fast lookups).
typedef struct cache_node {
    char *path;                // I store the file path as the lookup key.
    char *data;                // I keep a pointer to the actual cached data.
    size_t len;                // I need to know how many bytes are in 'data'.
    struct cache_node *prev;   // This points to the previous node in my LRU list.
    struct cache_node *next;   // This points to the next node in my LRU list.
    struct cache_node *hnext;  // This is for the hash table - it points to the next node in the same bucket.
} cache_node_t;

// I need to initialize the cache system before using it.
// This function sets up everything: the hash table, the lock, and the size limit.
int cache_init(size_t max_size_bytes);

// When the program is shutting down, I need to clean up all cache resources.
// This function frees all memory and destroys the synchronization primitives.
void cache_destroy();

// This is how clients retrieve data from the cache.
// If the data is found (a "hit"), I return 0 and provide the data.
// If it's not found (a "miss"), I return -1.
int cache_get(const char *path, char **out_buf, size_t *out_len);

// This is how clients store data in the cache.
// I'll either create a new entry or update an existing one.
// I also handle LRU eviction if the cache gets too full.
int cache_put(const char *path, const char *buf, size_t len);

#endif 