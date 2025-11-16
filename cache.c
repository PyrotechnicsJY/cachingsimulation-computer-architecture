#include "cache.h"

#include <stdlib.h>
#include <string.h>

/*
 * Local helpers
 */

static int is_power_of_two_u32(uint32_t x) {
    return x && ((x & (x - 1)) == 0);
}

static unsigned ilog2_u32(uint32_t x) {
    // Assumes x is a power of two and x > 0
    unsigned n = 0;
    while (x > 1) {
        x >>= 1;
        ++n;
    }
    return n;
}

/*
 * Initialize cache data structure based on Config.
 *
 * NOTE: This sets up all the fields and allocates memory, but does NOT
 * perform any actual cache simulation. That's for Milestone 3.
 *
 * Returns:
 *   0  on success
 *  -1  on error (bad params or allocation failure)
 */
int cache_init(Cache *cache, const Config *cfg) {
    if (!cache || !cfg) {
        return -1;
    }

    memset(cache, 0, sizeof(*cache));

    // Basic configuration from CLI Config
    cache->cache_size_bytes = (uint32_t)cfg->cache_kb * 1024u;
    cache->block_size       = (uint32_t)cfg->block_bytes;
    cache->associativity    = (uint32_t)cfg->associativity;
    cache->policy           = cfg->policy;

    // Sanity checks – these should already be enforced in parse_args()
    if (!is_power_of_two_u32(cache->cache_size_bytes) ||
        !is_power_of_two_u32(cache->block_size)       ||
        (cache->associativity == 0)                   ||
        (cache->cache_size_bytes == 0)) {
        return -1;
    }

    // Number of sets = total cache bytes / (block_size * associativity)
    uint64_t denom = (uint64_t)cache->block_size * (uint64_t)cache->associativity;
    if (denom == 0 || (cache->cache_size_bytes % denom) != 0) {
        return -1;
    }

    cache->num_sets = (uint32_t)(cache->cache_size_bytes / denom);

    if (!is_power_of_two_u32(cache->num_sets)) {
        return -1;
    }

    // Bit counts (32-bit physical addresses)
    cache->offset_bits = ilog2_u32(cache->block_size);
    cache->index_bits  = ilog2_u32(cache->num_sets);
    cache->tag_bits    = 32u - cache->offset_bits - cache->index_bits;

    // Allocate sets
    cache->sets = (CacheSet *)calloc(cache->num_sets, sizeof(CacheSet));
    if (!cache->sets) {
        return -1;
    }

    // Allocate lines for each set
    for (uint32_t i = 0; i < cache->num_sets; ++i) {
        cache->sets[i].lines = (CacheLine *)calloc(cache->associativity,
                                                   sizeof(CacheLine));
        if (!cache->sets[i].lines) {
            // Cleanup already allocated sets and fail
            for (uint32_t j = 0; j < i; ++j) {
                free(cache->sets[j].lines);
                cache->sets[j].lines = NULL;
            }
            free(cache->sets);
            cache->sets = NULL;
            return -1;
        }
        cache->sets[i].rr_next = 0;
        // calloc already zeroes valid/ever_used/tag
    }

    return 0;
}

/*
 * Free all memory associated with the cache and reset fields.
 */
void cache_free(Cache *cache) {
    if (!cache) return;

    if (cache->sets) {
        for (uint32_t i = 0; i < cache->num_sets; ++i) {
            free(cache->sets[i].lines);
            cache->sets[i].lines = NULL;
        }
        free(cache->sets);
        cache->sets = NULL;
    }

    memset(cache, 0, sizeof(*cache));
}

/*
 * Stub for cache access.
 *
 * For Milestone 2: this just bumps the access counter (if provided) and
 * returns success. No hits/misses are tracked yet.
 *
 * For Milestone 3: we will:
 *   - Decode addr into tag/index/offset
 *   - Search the appropriate set
 *   - Update stats->hits/misses/compulsory_misses/conflict_misses
 *   - Use RR/RND replacement when needed
 */
int cache_access(Cache *cache, CacheStats *stats, paddr_t addr, uint32_t size) {
    (void)addr;  // unused for now
    (void)size;  // unused for now

    if (!cache || !stats) {
        return -1;
    }

    stats->accesses++;

    // TODO (Milestone 3): implement real cache behavior here.

    return 0;
}

/*
 * Stub for invalidating entries that map to a given physical page.
 *
 * For Milestone 2: no-op.
 *
 * For Milestone 3: we may use this to:
 *   - Invalidate cache lines when a physical page is freed/swapped
 *   - Invalidate cache lines when a process exits, etc.
 */
void cache_invalidate_page(Cache *cache, uint32_t phys_page_num, uint32_t page_size) {
    (void)cache;
    (void)phys_page_num;
    (void)page_size;

    // TODO (Milestone 3): walk cache and invalidate lines that map to this page.
}
