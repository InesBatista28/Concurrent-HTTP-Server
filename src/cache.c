#define _XOPEN_SOURCE 700 // I need this for POSIX extensions and thread-safe operations.

#include "cache.h"
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <stdio.h>
#include <bits/pthreadtypes.h>

// * Global Cache State
// I'm keeping all cache data as static variables because I want a single, globally accessible cache.
// My cache is thread-safe, protected by a read-write lock that allows multiple concurrent readers.
// I implement an LRU (Least Recently Used) policy using a doubly-linked list for ordering
// combined with a hash table for O(1) lookups.
static cache_node_t **htable = NULL;    // I'll store pointers to hash table buckets here.
static size_t hsize = 0;                // I need to remember how many buckets I have.
static cache_node_t *head = NULL;       // This points to the MRU (Most Recently Used) end of my list.
static cache_node_t *tail = NULL;       // This points to the LRU (Least Recently Used) end of my list.
static size_t current_size = 0;         // I'm tracking the total size of cached data in bytes.
static size_t max_size = 0;             // This is my limit - I can't exceed this many bytes.
static pthread_rwlock_t cache_lock = PTHREAD_RWLOCK_INITIALIZER; // My read-write lock for thread safety.

// I need a good hash function to distribute keys across my hash table.
// I'm using the djb2 algorithm because it's simple and works well for strings.
// I'm taking a string path and converting it to a hash value.
static unsigned long hash_str(const char *s)
{
    unsigned long h = 5381; // This is the magic seed value for djb2.
    int c;
    
    // I'm iterating through each character of the string.
    // The formula is: hash * 33 + c, but I'm using bit shifting for efficiency.
    while ((c = *s++)) {
        h = ((h << 5) + h) + (unsigned long)c; // h * 33 + c
    }
    return h;
}

// I need to make sure my hash table is allocated before I use it.
// This helper function checks if the table exists and creates it if it doesn't.
static int ensure_table(size_t size)
{
    if (htable) return 0; // If the table already exists, I'm good to go.
    
    hsize = size; // I'll remember the size I chose.
    htable = calloc(hsize, sizeof(cache_node_t *)); // I'm using calloc to get zeroed memory.
    return htable ? 0 : -1; // I return 0 on success, -1 if allocation failed.
}

// This is where I set up my cache system.
// I need to initialize everything: the lock, the hash table, and set my size limits.
int cache_init(size_t max_size_bytes)
{
    max_size = max_size_bytes; // I'm setting my maximum cache size.
    current_size = 0; // Starting with an empty cache.
    head = tail = NULL; // No nodes in my list yet.
    
    // I need to initialize the read-write lock for thread safety.
    if (pthread_rwlock_init(&cache_lock, NULL) != 0) {
        return -1; // If I can't create the lock, something's wrong.
    }
    
    // I'm creating a hash table with 4096 buckets. This is a good default size.
    return ensure_table(4096);
}

// When it's time to clean up, I need to free everything.
// This function destroys the cache completely, freeing all memory.
void cache_destroy()
{
    if (!htable) return; // If there's no cache, I have nothing to do.
    
    // I need exclusive access because I'm about to tear everything down.
    pthread_rwlock_wrlock(&cache_lock);
    
    // I'm going through each bucket in my hash table.
    for (size_t i = 0; i < hsize; i++) {
        cache_node_t *n = htable[i];
        
        // For each bucket, I'm freeing all the nodes in the chain.
        while (n) {
            cache_node_t *next = n->hnext; // I save the next pointer before freeing.
            
            // I need to free all the dynamically allocated parts of the node.
            free(n->path);  // The path string.
            free(n->data);  // The cached data.
            free(n);        // The node structure itself.
            
            n = next; // Move to the next node in the chain.
        }
        htable[i] = NULL; // This bucket is now empty.
    }
    
    // Now I can free the hash table itself.
    free(htable);
    htable = NULL;
    
    // Reset all my global pointers.
    head = tail = NULL;
    current_size = 0;
    
    // I'm done modifying, so I can release the lock.
    pthread_rwlock_unlock(&cache_lock);
    
    // Finally, I destroy the lock itself since I won't need it anymore.
    pthread_rwlock_destroy(&cache_lock);
}

// This is an internal helper to remove a node from my doubly-linked list.
// I'm not exposing this function because callers shouldn't mess with my list directly.
static void remove_from_list(cache_node_t *n)
{
    if (!n) return; // If there's no node, I have nothing to do.
    
    // I need to update the node's neighbors to point to each other.
    if (n->prev) {
        n->prev->next = n->next; // The previous node now skips over me.
    } else {
        head = n->next; // If I was the head, the next node becomes the new head.
    }
    
    if (n->next) {
        n->next->prev = n->prev; // The next node now points back to my previous.
    } else {
        tail = n->prev; // If I was the tail, the previous node becomes the new tail.
    }
    
    // I isolate the node completely.
    n->prev = n->next = NULL;
}

// This helper inserts a node at the front of my list, making it the Most Recently Used.
static void insert_at_head(cache_node_t *n)
{
    n->prev = NULL; // Nothing comes before the head.
    n->next = head; // My current head becomes second in line.
    
    if (head) {
        head->prev = n; // The old head now points back to me.
    } else {
        tail = n; // If the list was empty, I'm also the tail.
    }
    
    head = n; // I'm the new head now!
}

// When my cache gets too big, I need to evict some items.
// I always evict from the tail because that's where the Least Recently Used items are.
static void evict_if_needed()
{
    // I keep removing tail nodes until my cache is within the size limit.
    while (current_size > max_size && tail) {
        cache_node_t *n = tail; // This is the item I'm going to evict.
        
        // First, I need to remove it from the hash table.
        unsigned long h = hash_str(n->path) % hsize; // Find which bucket it's in.
        cache_node_t *prev = NULL;
        cache_node_t *iter = htable[h];
        
        // I'm searching through the hash chain to find this node.
        while (iter) {
            if (iter == n) { // Found it!
                if (prev) {
                    prev->hnext = iter->hnext; // Skip over me in the chain.
                } else {
                    htable[h] = iter->hnext; // I was the first in the bucket.
                }
                break;
            }
            prev = iter;
            iter = iter->hnext;
        }
        
        // Now remove it from the LRU list.
        remove_from_list(n);
        
        // Update my size tracker.
        current_size -= n->len;
        
        // Free all the memory associated with this node.
        free(n->path);
        free(n->data);
        free(n);
    }
}

// This is the main function for getting data from the cache.
// When someone asks for a file, I check if I have it cached.
int cache_get(const char *path, char **out_buf, size_t *out_len)
{
    if (!htable) return -1; // If cache isn't initialized, I can't help.
    
    unsigned long h = hash_str(path) % hsize; // Figure out which bucket to check.
    
    // I start with a read lock because I'm just looking, not modifying.
    if (pthread_rwlock_rdlock(&cache_lock) != 0) return -1;
    
    // I'm searching through the hash chain in this bucket.
    cache_node_t *n = htable[h];
    while (n) {
        if (strcmp(n->path, path) == 0) break; // Found it!
        n = n->hnext;
    }
    
    if (!n) {
        // Cache miss - the file isn't in my cache.
        pthread_rwlock_unlock(&cache_lock);
        return -1;
    }
    
    // Cache hit! But now I have a problem...
    // I found the item with a read lock, but I need to promote it to MRU,
    // which requires modifying the list. So I need to upgrade to a write lock.
    
    pthread_rwlock_unlock(&cache_lock); // Release the read lock first.
    
    // Now I acquire a write lock.
    if (pthread_rwlock_wrlock(&cache_lock) != 0) return -1;
    
    // IMPORTANT: Between releasing the read lock and getting the write lock,
    // another thread might have modified the cache. So I need to search again.
    cache_node_t *n2 = htable[h];
    while (n2) {
        if (strcmp(n2->path, path) == 0) break;
        n2 = n2->hnext;
    }
    
    if (!n2) {
        // The item disappeared while I was switching locks!
        pthread_rwlock_unlock(&cache_lock);
        return -1;
    }
    
    // Now I can safely promote this node to Most Recently Used.
    remove_from_list(n2);    // Take it out of its current position.
    insert_at_head(n2);      // Put it at the front of the list.
    
    // I need to return a copy of the data, not the original pointer.
    // This way the caller can use it without worrying about thread safety.
    char *buf = malloc(n2->len);
    if (!buf) {
        pthread_rwlock_unlock(&cache_lock);
        return -1;
    }
    memcpy(buf, n2->data, n2->len);
    
    // Give the caller what they asked for.
    *out_buf = buf;
    *out_len = n2->len;
    
    pthread_rwlock_unlock(&cache_lock);
    return 0; // Success!
}

// This function adds or updates items in the cache.
int cache_put(const char *path, const char *buf, size_t len)
{
    if (!htable) return -1; // Cache not initialized.
    if (len == 0 || !buf) return -1; // Invalid parameters.
    
    // I'm setting a hard limit: no single file larger than 1MB can be cached.
    // This prevents one large file from hogging all the cache space.
    if (len > (1 * 1024 * 1024)) return -1;
    
    // I need a write lock immediately because I'm going to modify the cache.
    if (pthread_rwlock_wrlock(&cache_lock) != 0) return -1;
    
    unsigned long h = hash_str(path) % hsize;
    
    // First, check if this path is already in the cache.
    cache_node_t *n = htable[h];
    while (n) {
        if (strcmp(n->path, path) == 0) break;
        n = n->hnext;
    }
    
    if (n) {
        // The item already exists - I need to update it.
        current_size -= n->len; // Remove the old size from my total.
        
        // Free the old data and allocate space for the new data.
        free(n->data);
        n->data = malloc(len);
        if (!n->data) {
            pthread_rwlock_unlock(&cache_lock);
            return -1;
        }
        
        // Copy the new data in.
        memcpy(n->data, buf, len);
        n->len = len;
        current_size += len; // Add the new size to my total.
        
        // Since this item was just used, I promote it to MRU.
        remove_from_list(n);
        insert_at_head(n);
        
        // The cache might now be too big, so I check if I need to evict anything.
        evict_if_needed();
        
        pthread_rwlock_unlock(&cache_lock);
        return 0; // Update successful.
    }
    
    // The item doesn't exist yet - I need to create a new cache entry.
    cache_node_t *node = malloc(sizeof(cache_node_t));
    if (!node) {
        pthread_rwlock_unlock(&cache_lock);
        return -1;
    }
    
    // I need to copy the path and data because the caller might free them later.
    node->path = strdup(path);
    node->data = malloc(len);
    
    // Check if both allocations succeeded.
    if (!node->path || !node->data) {
        free(node->path);
        free(node->data);
        free(node);
        pthread_rwlock_unlock(&cache_lock);
        return -1;
    }
    
    // Copy the actual data.
    memcpy(node->data, buf, len);
    node->len = len;
    
    // Set up the node's links.
    node->prev = node->next = NULL;
    node->hnext = htable[h]; // Insert at the beginning of the hash chain.
    htable[h] = node;
    
    // Add to the front of the LRU list (it's now the Most Recently Used).
    insert_at_head(node);
    current_size += len; // Update my size counter.
    
    // Check if adding this item made the cache too big.
    evict_if_needed();
    
    pthread_rwlock_unlock(&cache_lock);
    return 0; // Successfully added to cache.
}