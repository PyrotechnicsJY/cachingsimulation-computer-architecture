#ifndef VIRTUAL_MEMORY_H
#define VIRTUAL_MEMORY_H

#include <stdint.h>

#include "calculations.h"

#define PAGESIZE 4096
#define NUM_VPAGES (512  * 1024)

typedef struct {
  uint32_t ppn;
  uint8_t valid;
}
page_table_entry;

typedef struct {
  page_table_entry * entries;
  int max_size_pagetable;
  int curr_size_pagetable;
}
page_table;

typedef struct {
  uint32_t * freelist;
  int num_free_pages;
  int max_phys_pages;
}
physical_memory;

typedef struct {
  int pg_hits;
  int pg_free;
  int pg_fault;
}
statistics;

page_table * make_page_table(int amountofprocesses);
void free_page_tables(page_table * pagetables, int amountofprocesses);

physical_memory * make_physical_memory(Results * r);
void free_physical_memory(physical_memory * phys_mem);

uint32_t vm_translate_addr(int pid,
  uint32_t vaddr,
  page_table * pt_array,
  physical_memory * phys_mem,
  statistics * stats);

void process_trace_vm(Config * cfg,
  Results * res,
  page_table * pt_array,
  physical_memory * phys_mem,
  statistics * stats_per_proc);

void print_vm_results(Config * cfg,
  Results * res,
  page_table * pt_array,
  statistics * stats_per_proc);

#endif