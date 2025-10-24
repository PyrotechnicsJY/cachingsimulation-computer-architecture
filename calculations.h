#ifndef CALCULATIONS_H
#define CALCULATIONS_H

#include <stdint.h>

#define MAX_TRACES 3
#define MIN_CACHE_KB 8
#define MAX_CACHE_KB 8192
#define MIN_PHYS_MB 128
#define MAX_PHYS_MB 4096

typedef enum { RP_RR, RP_RND } repl_policy_t;

typedef struct {
    // CLI inputs
    int cache_kb;             // -s (KB)
    int block_bytes;          // -b (bytes)
    int associativity;        // -a (ways)
    repl_policy_t policy;     // -r (rr|rnd)
    int phys_mb;              // -p (MB)
    double os_percent;        // -u (0..100)
    long long time_slice;     // -n (>=1 or -1)
    char *traces[MAX_TRACES]; // -f (1..3)
    int num_traces;
} Config;

typedef struct {
    // Cache calculated values
    uint64_t blocks_total;
    int tag_bits;
    int index_bits;
    uint64_t rows_total;
    uint64_t overhead_bytes;
    uint64_t impl_mem_bytes;   // bytes
    double impl_kb;            // KB
    double cost_usd;

    // Physical memory calculated values
    uint64_t phys_pages;
    uint64_t sys_pages;
    int pte_bits;
    uint64_t pgt_total_bytes;
} Results;

// Parse CLI args into cfg and validate (exits with usage on error)
void parse_args(int argc, char **argv, Config *cfg);

// Compute all milestone #1 results from inputs
void compute_results(const Config *cfg, Results *out);

#endif // CALCULATIONS_H
