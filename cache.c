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

    // Bit counts (assume 32-bit physical addresses)
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
 * Flush the entire cache: mark all lines invalid.
 */
void cache_flush(Cache *cache) {
    if (!cache || !cache->sets) return;
    for (uint32_t i = 0; i < cache->num_sets; ++i) {
        CacheSet *set = &cache->sets[i];
        set->rr_next = 0;
        for (uint32_t w = 0; w < cache->associativity; ++w) {
            set->lines[w].valid = 0;
            // keep ever_used as-is so "unused blocks" metric still works
        }
    }
}

/*
 * Internal helper: perform a single-block access for the block that
 * contains 'block_addr' (which should be aligned to block_size).
 */
static void cache_access_block(Cache *cache, CacheStats *stats, paddr_t block_addr) {
    // Decode physical address into tag and index
    uint32_t block_index_addr = block_addr >> cache->offset_bits;
    uint32_t index = block_index_addr & (cache->num_sets - 1u);
    uint32_t tag   = block_index_addr >> cache->index_bits;

    CacheSet *set = &cache->sets[index];

    stats->accesses++;

    // Look for a hit
    for (uint32_t w = 0; w < cache->associativity; ++w) {
        CacheLine *line = &set->lines[w];
        if (line->valid && line->tag == tag) {
            // Hit
            stats->hits++;
            stats->total_cycles += 1; // cache hit = 1 cycle
            return;
        }
    }

    // Miss
    stats->misses++;

    // Determine if there is an invalid line => compulsory miss
    int invalid_index = -1;
    for (uint32_t w = 0; w < cache->associativity; ++w) {
        CacheLine *line = &set->lines[w];
        if (!line->valid) {
            invalid_index = (int)w;
            break;
        }
    }

    uint32_t victim_way;
    if (invalid_index >= 0) {
        // Compulsory miss: choose the first invalid line
        stats->compulsory_misses++;
        victim_way = (uint32_t)invalid_index;
    } else {
        // Conflict miss: all lines are valid => choose victim via policy
        stats->conflict_misses++;
        if (cache->policy == RP_RND) {
            victim_way = (uint32_t)(rand() % (int)cache->associativity);
        } else {
            // Round-robin
            victim_way = cache->sets[index].rr_next;
            cache->sets[index].rr_next =
                (cache->sets[index].rr_next + 1u) % cache->associativity;
        }
    }

    CacheLine *victim = &set->lines[victim_way];
    victim->valid = 1;
    victim->tag   = tag;
    victim->ever_used = 1;

    // Miss penalty: number of memory reads = ceil(block_size / 4),
    // each memory read costs 4 cycles.
    uint32_t reads = (cache->block_size + 4u - 1u) / 4u;
    stats->total_cycles += (uint64_t)reads * 4u;
}

/*
 * Simulate a memory access of 'size' bytes starting at physical 'addr'.
 *
 * We treat a "cache access" as one touch per cache row (block). So if the
 * access spans N distinct blocks, that results in N accesses.
 */
int cache_access(Cache *cache, CacheStats *stats, paddr_t addr, uint32_t size) {
    if (!cache || !stats || size == 0) {
        return -1;
    }

    uint32_t block_size = cache->block_size;

    // Align to first block covering 'addr'
    uint32_t first_block_addr = addr & ~(block_size - 1u);
    uint32_t last_addr = addr + size - 1u;
    uint32_t last_block_addr = last_addr & ~(block_size - 1u);

    for (uint32_t block_addr = first_block_addr;
         block_addr <= last_block_addr;
         block_addr += block_size) {
        cache_access_block(cache, stats, block_addr);
    }

    return 0;
}
