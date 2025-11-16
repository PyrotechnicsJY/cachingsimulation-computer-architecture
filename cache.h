#ifndef CACHE_H
#define CACHE_H

#include <stdint.h>
#include "calculations.h"

// Physical address type (32-bit)
typedef uint32_t paddr_t;

// A single cache line (one block)
typedef struct {
    uint8_t  valid;      // 1 = line contains valid data, 0 = invalid
    uint32_t tag;        // tag bits from the physical address
    uint8_t  ever_used;  // for "unused blocks" stats (0 = never filled)
} CacheLine;

// A set (row) in a set-associative cache
typedef struct {
    CacheLine *lines;    // dynamically allocated array of [associativity] lines
    uint32_t   rr_next;  // next way to evict for round-robin policy
} CacheSet;

// The whole cache
typedef struct {
    // Configuration (derived from Config)
    uint32_t       cache_size_bytes;  // total cache capacity in bytes
    uint32_t       block_size;        // bytes per block
    uint32_t       associativity;     // number of ways
    uint32_t       num_sets;          // number of sets (rows)

    uint32_t       index_bits;        // number of index bits
    uint32_t       offset_bits;       // number of block-offset bits
    uint32_t       tag_bits;          // number of tag bits

    repl_policy_t  policy;            // RP_RR or RP_RND

    // Dynamic storage
    CacheSet      *sets;              // array of [num_sets] sets
} Cache;

// Cache statistics (used for Milestone #3)
typedef struct {
    uint64_t accesses;           // total cache accesses
    uint64_t hits;               // hits
    uint64_t misses;             // misses
    uint64_t compulsory_misses;  // first reference to a block
    uint64_t conflict_misses;    // miss to a set that already has only valid lines
} CacheStats;

// Initialization / teardown (implement later)
int  cache_init(Cache *cache, const Config *cfg);
void cache_free(Cache *cache);

// Access & maintenance (implement later)
int  cache_access(Cache *cache, CacheStats *stats, paddr_t addr, uint32_t size);
void cache_invalidate_page(Cache *cache, uint32_t phys_page_num, uint32_t page_size);

#endif // CACHE_H
