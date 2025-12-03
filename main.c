#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "calculations.h"
#include "virtual_memory.h"
#include "cache.h"

static void print_report(const Config *c, const Results *r) {
    printf("Cache Simulator - CS 3853 - Team #1\n\n");

    printf("Trace File(s):\n");
    for (int i = 0; i < c->num_traces; ++i) {
        printf("\t%s\n", c->traces[i]);
    }

    //MILESTONE #1:  Input Parameters and Calculated Values

    printf("***** Cache Input Parameters *****\n\n");
    printf("Cache Size: %d KB\n", c->cache_kb);
    printf("Block Size: %d bytes\n", c->block_bytes);
    printf("Associativity: %d\n", c->associativity);
    printf("Replacement Policy: %s\n",
           (c->policy == RP_RR ? "Round Robin" : "Random"));
    printf("Physical Memory: %d MB\n", c->phys_mb);
    printf("Percent Memory Used by System: %.1f%%\n", c->os_percent);
    if (c->time_slice == -1) {
        printf("Instructions / Time Slice: All\n");
    } else {
        printf("Instructions / Time Slice: %lld\n", c->time_slice);
    }

    printf("\n***** Cache Calculated Values *****\n\n");
    printf("Total # Blocks: %llu\n", (unsigned long long)r->blocks_total);
    printf("Tag Size: %d bits\n", r->tag_bits);
    printf("Index Size: %d bits\n", r->index_bits);
    printf("Total # Rows: %llu\n", (unsigned long long)r->rows_total);
    printf("Overhead Size: %llu bytes\n",
           (unsigned long long)r->overhead_bytes);
    printf("Implementation Memory Size: %.2f KB (%llu bytes)\n",
           r->impl_kb, (unsigned long long)r->impl_mem_bytes);
    printf("Cost: $%.2f @ $0.07 per KB\n", r->cost_usd);

    printf("\n***** Physical Memory Calculated Values *****\n\n");
    printf("Number of Physical Pages: %llu\n",
           (unsigned long long)r->phys_pages);
    printf("Number of Pages for System: %llu\n",
           (unsigned long long)r->sys_pages);
    printf("Size of Page Table Entry: %d bits\n", r->pte_bits);
    printf("Total RAM for Page Table(s): %llu bytes\n",
           (unsigned long long)r->pgt_total_bytes);
}

static void print_cache_results(const Config *cfg,
                                const Results *res,
                                const Cache *cache,
                                const CacheStats *cs,
                                const statistics *vm_stats_per_proc) {
    (void)cache; // cache geometry is already reflected in Results

    // Aggregate page faults for CPI (100 cycles each)
    uint64_t total_faults = 0;
    for (int i = 0; i < cfg->num_traces; ++i) {
        total_faults += (uint64_t)vm_stats_per_proc[i].pg_fault;
    }

    uint64_t total_cycles = cs->total_cycles + total_faults * 100ULL;

    double hit_rate  = (cs->accesses > 0)
                       ? (100.0 * (double)cs->hits / (double)cs->accesses)
                       : 0.0;
    double miss_rate = 100.0 - hit_rate;

    double cpi = (cs->instr_count > 0)
                 ? ((double)total_cycles / (double)cs->instr_count)
                 : 0.0;

    // Unused cache blocks: count lines that were never filled
    uint64_t unused_blocks = 0;
    if (cache && cache->sets) {
        for (uint32_t i = 0; i < cache->num_sets; ++i) {
            const CacheSet *set = &cache->sets[i];
            for (uint32_t w = 0; w < cache->associativity; ++w) {
                if (!set->lines[w].ever_used) {
                    unused_blocks++;
                }
            }
        }
    }

    // Compute unused KB based on "unused blocks" and per-block size+overhead
    double unused_kb = 0.0;
    if (res->blocks_total > 0) {
        double overhead_per_block =
            (double)res->overhead_bytes / (double)res->blocks_total;
        unused_kb = (double)unused_blocks *
                    ((double)cfg->block_bytes + overhead_per_block) / 1024.0;
    }
    double percent_unused = (res->impl_kb > 0.0)
                            ? (unused_kb * 100.0 / res->impl_kb)
                            : 0.0;
    double waste_dollars = unused_kb * 0.07; // $0.07 per KB

    //MILESTONE #3: - Cache Simulation Results

    printf("***** CACHE SIMULATION RESULTS *****\n\n");
    printf("Total Cache Accesses:   %llu\n",
           (unsigned long long)cs->accesses);
    printf("--- Instruction Bytes: %llu\n",
           (unsigned long long)cs->instr_bytes);
    printf("--- SrcDst Bytes:  %llu\n",
           (unsigned long long)cs->srcdst_bytes);
    printf("Cache Hits:             %llu\n",
           (unsigned long long)cs->hits);
    printf("Cache Misses:           %llu\n",
           (unsigned long long)cs->misses);
    printf("--- Compulsory Misses:  %llu\n",
           (unsigned long long)cs->compulsory_misses);
    printf("--- Conflict Misses:    %llu\n",
           (unsigned long long)cs->conflict_misses);

    printf("\n\n***** *****  CACHE HIT & MISS RATE:  ***** *****\n\n");
    printf("Hit  Rate:              %.4f%%\n", hit_rate);
    printf("Miss Rate:              %.4f%%\n", miss_rate);
    printf("CPI:                    %.2f Cycles/Instruction  (%llu)\n",
           cpi, (unsigned long long)total_cycles);
    printf("Unused Cache Space:     %.2f KB / %.2f KB = %.2f%%  Waste: $%.2f/chip\n",
           unused_kb, res->impl_kb, percent_unused, waste_dollars);
    printf("Unused Cache Blocks:    %llu / %llu\n",
           (unsigned long long)unused_blocks,
           (unsigned long long)res->blocks_total);
}

int main(int argc, char **argv) {
    Config  cfg;
    Results res;

    parse_args(argc, argv, &cfg);
    compute_results(&cfg, &res);

    // Allocate virtual memory structures
    page_table *ptables = make_page_table(cfg.num_traces);
    if (!ptables) {
        return EXIT_FAILURE;
    }

    physical_memory *phys_mem = make_physical_memory(&res);
    if (!phys_mem) {
        free_page_tables(ptables, cfg.num_traces);
        return EXIT_FAILURE;
    }

    statistics *vm_stats = calloc(cfg.num_traces, sizeof(statistics));
    if (!vm_stats) {
        free_physical_memory(phys_mem);
        free_page_tables(ptables, cfg.num_traces);
        return EXIT_FAILURE;
    }

    // Initialize cache
    Cache cache;
    CacheStats cstats;
    memset(&cache, 0, sizeof(cache));
    memset(&cstats, 0, sizeof(cstats));

    if (cache_init(&cache, &cfg) != 0) {
        fprintf(stderr, "Error: cache_init failed\n");
        free(vm_stats);
        free_physical_memory(phys_mem);
        free_page_tables(ptables, cfg.num_traces);
        return EXIT_FAILURE;
    }

    print_report(&cfg, &res);

    // Process traces: VM + cache + CPI stats
    process_trace_vm(&cfg, &res, ptables, phys_mem, vm_stats, &cache, &cstats);

    // Print VM (milestone #2) results
    print_vm_results(&cfg, &res, ptables, vm_stats);

    // Print cache (milestone #3) results
    print_cache_results(&cfg, &res, &cache, &cstats, vm_stats);

    cache_free(&cache);
    free(vm_stats);
    free_physical_memory(phys_mem);
    free_page_tables(ptables, cfg.num_traces);

    return 0;
}
