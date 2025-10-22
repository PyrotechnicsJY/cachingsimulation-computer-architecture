#include <stdio.h>
#include "calculations.h"

static void print_report(const Config *c, const Results *r) {
    printf("Cache Simulator - CS 3853 – Team #XX\n");
    printf("Trace File(s):\n");
    for (int i = 0; i < c->num_traces; ++i) {
        printf("%s\n", c->traces[i]);
    }

    printf("***** Cache Input Parameters *****\n");
    printf("Cache Size: %d KB\n", c->cache_kb);
    printf("Block Size: %d bytes\n", c->block_bytes);
    printf("Associativity: %d\n", c->associativity);
    printf("Replacement Policy: %s\n", (c->policy == RP_RR ? "Round Robin" : "Random"));
    printf("Physical Memory: %d MB\n", c->phys_mb);
    printf("Percent Memory Used by System: %.1f%%\n", c->os_percent);
    if (c->time_slice == -1) printf("Instructions / Time Slice: All\n");
    else printf("Instructions / Time Slice: %lld\n", c->time_slice);

    printf("***** Cache Calculated Values *****\n");
    printf("Total # Blocks: %llu\n", (unsigned long long)r->blocks_total);
    printf("Tag Size: %d bits (based on actual physical memory)\n", r->tag_bits);
    printf("Index Size: %d bits\n", r->index_bits);
    printf("Total # Rows: %llu\n", (unsigned long long)r->rows_total);
    printf("Overhead Size: %llu bytes\n", (unsigned long long)r->overhead_bytes);
    printf("Implementation Memory Size: %.2f KB (%llu bytes)\n",
           r->impl_kb, (unsigned long long)r->impl_mem_bytes);
    printf("Cost: $%.2f @ $0.07 per KB\n", r->cost_usd);

    printf("***** Physical Memory Calculated Values *****\n");
    printf("Number of Physical Pages: %llu\n", (unsigned long long)r->phys_pages);
    printf("Number of Pages for System: %llu ( %.2f * %llu = %llu )\n",
           (unsigned long long)r->sys_pages, 
           (double)(c->os_percent/100.0),
           (unsigned long long)r->phys_pages,
           (unsigned long long)r->sys_pages);
    printf("Size of Page Table Entry: %d bits (1 valid bit, %d for PhysPage)\n",
           r->pte_bits, r->pte_bits - 1);
    printf("Total RAM for Page Table(s): %llu bytes (512K entries * %d .trc files * %d / 8)\n",
           (unsigned long long)r->pgt_total_bytes, c->num_traces, r->pte_bits);
}

int main(int argc, char **argv) {
    Config cfg;
    Results res;

    parse_args(argc, argv, &cfg);
    compute_results(&cfg, &res);
    print_report(&cfg, &res);
    return 0;
}
