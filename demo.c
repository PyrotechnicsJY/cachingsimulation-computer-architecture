#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <math.h>

#define MAX_TRACES 3
#define MIN_CACHE_KB 8
#define MAX_CACHE_KB 8192
#define MIN_PHYS_MB 128
#define MAX_PHYS_MB 4096

typedef enum { RP_RR, RP_RND } repl_policy_t;

typedef struct {
    // inputs
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

// ---------- helpers ----------
static int is_power_of_two_u64(uint64_t x) { return x && ((x & (x - 1)) == 0); }

static int ilog2_u64(uint64_t x) {
    // assumes x is power of two
    int n = 0;
    while ((x >>= 1) != 0) n++;
    return n;
}

static void to_lower_str(char *s) {
    for (; *s; ++s) *s = (char)tolower((unsigned char)*s);
}

static void die(const char *msg) {
    fprintf(stderr, "Error: %s\n", msg);
    exit(EXIT_FAILURE);
}


static int parse_int(const char *s, int *out) {
    char *end = NULL;
    errno = 0;
    long v = strtol(s, &end, 10);
    if (errno || end == s || *end != '\0') return 0;
    if (v < INT32_MIN || v > INT32_MAX) return 0;
    *out = (int)v;
    return 1;
}

static int parse_ll(const char *s, long long *out) {
    char *end = NULL;
    errno = 0;
    long long v = strtoll(s, &end, 10);
    if (errno || end == s || *end != '\0') return 0;
    *out = v;
    return 1;
}

static int parse_double(const char *s, double *out) {
    char *end = NULL;
    errno = 0;
    double v = strtod(s, &end);
    if (errno || end == s || *end != '\0') return 0;
    *out = v;
    return 1;
}

static void usage_and_exit(const char *prog) {
    fprintf(stderr,
        "Usage: %s "
        "-s <cache KB 8..8192 pow2> "
        "-b <block 8|16|32|64> "
        "-a <assoc 1|2|4|8|16> "
        "-r <RR|RND> "
        "-p <phys MB 128..4096 pow2> "
        "-n <instructions or -1 for All> "
        "-u <OS %% 0..100> "
        "-f <trace1> [-f <trace2>] [-f <trace3>]\n",
        prog);
    exit(EXIT_FAILURE);
}

// ---------- parsing ----------
static void parse_args(int argc, char **argv, Config *cfg) {
    memset(cfg, 0, sizeof(*cfg));
    cfg->time_slice = -1; // default to All unless provided

    if (argc <= 1) usage_and_exit(argv[0]);

    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "-s") == 0) {
            if (++i >= argc) usage_and_exit(argv[0]);
            if (!parse_int(argv[i], &cfg->cache_kb)) die("Invalid -s value");
        } else if (strcmp(argv[i], "-b") == 0) {
            if (++i >= argc) usage_and_exit(argv[0]);
            if (!parse_int(argv[i], &cfg->block_bytes)) die("Invalid -b value");
        } else if (strcmp(argv[i], "-a") == 0) {
            if (++i >= argc) usage_and_exit(argv[0]);
            if (!parse_int(argv[i], &cfg->associativity)) die("Invalid -a value");
        } else if (strcmp(argv[i], "-r") == 0) {
            if (++i >= argc) usage_and_exit(argv[0]);
            char tmp[16]; snprintf(tmp, sizeof(tmp), "%s", argv[i]); to_lower_str(tmp);
            if (strcmp(tmp, "rr") == 0) cfg->policy = RP_RR;
            else if (strcmp(tmp, "rnd") == 0) cfg->policy = RP_RND;
            else die("Invalid -r (use RR or RND)");
        } else if (strcmp(argv[i], "-p") == 0) {
            if (++i >= argc) usage_and_exit(argv[0]);
            if (!parse_int(argv[i], &cfg->phys_mb)) die("Invalid -p value");
        } else if (strcmp(argv[i], "-u") == 0) {
            if (++i >= argc) usage_and_exit(argv[0]);
            if (!parse_double(argv[i], &cfg->os_percent)) die("Invalid -u value");
        } else if (strcmp(argv[i], "-n") == 0) {
            if (++i >= argc) usage_and_exit(argv[0]);
            if (!parse_ll(argv[i], &cfg->time_slice)) die("Invalid -n value");
        } else if (strcmp(argv[i], "-f") == 0) {
            if (++i >= argc) usage_and_exit(argv[0]);
            if (cfg->num_traces >= MAX_TRACES) die("Too many -f traces (max 3)");
            cfg->traces[cfg->num_traces++] = argv[i];
        } else {
            usage_and_exit(argv[0]);
        }
    }

    // Validate ranges / sets
    if (cfg->cache_kb < MIN_CACHE_KB || cfg->cache_kb > MAX_CACHE_KB ||
        !is_power_of_two_u64((uint64_t)cfg->cache_kb)) {
        die("-s must be a power of two KB in [8, 8192]");
    }

    if (!(cfg->block_bytes == 8 || cfg->block_bytes == 16 ||
          cfg->block_bytes == 32 || cfg->block_bytes == 64)) {
        die("-b must be 8, 16, 32, or 64 bytes");
    }

    if (!(cfg->associativity == 1 || cfg->associativity == 2 ||
          cfg->associativity == 4 || cfg->associativity == 8 ||
          cfg->associativity == 16)) {
        die("-a must be 1, 2, 4, 8, or 16");
    }

    if (cfg->phys_mb < MIN_PHYS_MB || cfg->phys_mb > MAX_PHYS_MB ||
        !is_power_of_two_u64((uint64_t)cfg->phys_mb)) {
        die("-p must be a power of two MB in [128, 4096]");
    }

    if (cfg->os_percent < 0.0 || cfg->os_percent > 100.0) {
        die("-u must be between 0 and 100");
    }

    if (cfg->time_slice < -1 || cfg->time_slice == 0) {
        die("-n must be -1 (All) or >= 1");
    }

    if (cfg->num_traces < 1) {
        die("Provide 1 to 3 trace files with -f");
    }
}

// ---------- calculations & output ----------
int main(int argc, char **argv) {
    Config c;
    parse_args(argc, argv, &c);

    // Core sizes
    const uint64_t cache_bytes = (uint64_t)c.cache_kb * 1024ULL;
    const uint64_t block_bytes = (uint64_t)c.block_bytes;
    const uint64_t blocks_total = cache_bytes / block_bytes; // total blocks (all ways combined)
    const int ways = c.associativity;
    const uint64_t rows_total = blocks_total / (uint64_t)ways;

    // Bits for cache addressing (based on *physical* addr bits)
    const uint64_t phys_bytes = (uint64_t)c.phys_mb * 1024ULL * 1024ULL;
    // phys address width = log2(phys_bytes)
    int phys_addr_bits = ilog2_u64(phys_bytes); // valid because phys_mb is power-of-two
    int block_off_bits = ilog2_u64(block_bytes);
    int index_bits = ilog2_u64(rows_total);
    int tag_bits = phys_addr_bits - index_bits - block_off_bits;
    if (tag_bits < 0) die("Invalid configuration: negative tag bits (check sizes).");

       // Overhead = (valid + tag) bits per line * ways * rows
    uint64_t per_line_bits = 1ULL + (uint64_t)tag_bits; // valid + tag per way
    uint64_t total_overhead_bits = rows_total * (uint64_t)ways * per_line_bits;
    uint64_t overhead_bytes = (total_overhead_bits + 7) / 8; // ceil to bytes

    uint64_t impl_mem_bytes = cache_bytes + overhead_bytes;

    // Cost at $0.07 / KB (1 KB = 1024 bytes)
    double impl_kb = (double)impl_mem_bytes / 1024.0;
    double cost = impl_kb * 0.07;

    // Physical memory / paging (4KB pages)
    const uint64_t page_bytes = 4096ULL;
    const uint64_t phys_pages = phys_bytes / page_bytes;
    double os_fraction = c.os_percent / 100.0;
    uint64_t sys_pages = (uint64_t)llround(os_fraction * (double)phys_pages);

    // PTE size: 1 valid + bits for PhysPage index
    int phys_page_bits = ilog2_u64(phys_pages);
    int pte_bits = 1 + phys_page_bits;

    // Virtual page table entries per process: 512K
    const uint64_t vpt_entries_per_proc = 512ULL * 1024ULL; // 512K entries
    uint64_t pgt_total_bits = vpt_entries_per_proc * (uint64_t)c.num_traces * (uint64_t)pte_bits;
    uint64_t pgt_total_bytes = pgt_total_bits / 8; // exact division in our case

    // ---------- Output ----------
    printf("Cache Simulator - CS 3853 - Team #1\n\n");
    printf("Trace File(s):\n");
    for (int i = 0; i < c.num_traces; ++i) {
        printf("\t%s\n", c.traces[i]);
    }

    printf("\n***** Cache Input Parameters *****\n\n");
    printf("Cache Size: %d KB\n", c.cache_kb);
    printf("Block Size: %d bytes\n", c.block_bytes);
    printf("Associativity: %d\n", c.associativity);
    printf("Replacement Policy: %s\n", (c.policy == RP_RR ? "Round Robin" : "Random"));
    printf("Physical Memory: %d MB\n", c.phys_mb);
    printf("Percent Memory Used by System: %.1f%%\n", c.os_percent);
    if (c.time_slice == -1) {
        printf("Instructions / Time Slice: All\n");
    } else {
        printf("Instructions / Time Slice: %lld\n", c.time_slice);
    }

    printf("\n***** Cache Calculated Values *****\n\n");
    printf("Total # Blocks: %llu\n", (unsigned long long)blocks_total);
    printf("Tag Size: %d bits\n", tag_bits);
    printf("Index Size: %d bits\n", index_bits);
    printf("Total # Rows: %llu\n", (unsigned long long)rows_total);
    printf("Overhead Size: %llu bytes\n", (unsigned long long)overhead_bytes);
    printf("Implementation Memory Size: %.2f KB (%llu bytes)\n", impl_kb, (unsigned long long)impl_mem_bytes);
    printf("Cost: $%.2f @ $0.07 per KB\n", cost);

    printf("\n***** Physical Memory Calculated Values *****\n\n");
    printf("Number of Physical Pages: %llu\n", (unsigned long long)phys_pages);
    // show the explanatory multiplication like the example
    printf("Number of Pages for System: %llu\n",(unsigned long long)sys_pages);
    printf("Size of Page Table Entry: %d bits\n", pte_bits);
    printf("Total RAM for Page Table(s): %llu bytes\n",(unsigned long long)pgt_total_bytes);

    return 0;
}
