#ifndef VIRTUAL_MEMORY_H
#define VIRTUAL_MEMORY_H

#include <stdint.h>

#include "calculations.h"
#include "cache.h"

#define PAGESIZE 4096
#define NUM_VPAGES (512 * 1024)

typedef struct {
    uint32_t ppn;  // physical page number
    uint8_t  valid;
} page_table_entry;

typedef struct {
    page_table_entry *entries;        // array of NUM_VPAGES entries
    int max_size_pagetable;           // number of entries allocated
    int curr_size_pagetable;          // number of valid entries
} page_table;

typedef struct {
    uint32_t *freelist;   // stack of free physical page numbers
    int num_free_pages;   // current number of free pages
    int max_phys_pages;   // total number of physical pages
} physical_memory;

typedef struct {
    int pg_hits;   // page table hits
    int pg_free;   // pages from free list
    int pg_fault;  // page faults (no free pages)
} statistics;

page_table      *make_page_table(int amountofprocesses);
void             free_page_tables(page_table *pagetables, int amountofprocesses);

physical_memory *make_physical_memory(Results *r);
void             free_physical_memory(physical_memory *phys_mem);

// Translate virtual address -> physical page number (PPN)
uint32_t vm_translate_addr(int pid,
                           uint32_t vaddr,
                           page_table *pt_array,
                           physical_memory *phys_mem,
                           statistics *stats);

// Process trace files: perform VM mapping and cache simulation.
void process_trace_vm(Config *cfg,
                      Results *res,
                      page_table *pt_array,
                      physical_memory *phys_mem,
                      statistics *stats_per_proc,
                      Cache *cache,
                      CacheStats *cstats);

// Print milestone #2 style virtual memory results.
void print_vm_results(Config *cfg,
                      Results *res,
                      page_table *pt_array,
                      statistics *stats_per_proc);

#endif
