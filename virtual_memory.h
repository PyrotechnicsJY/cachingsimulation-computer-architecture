#ifndef VIRTUAL_MEMORY_H
#define VIRTUAL_MEMORY_H
#include <stdint.h>
#include "calculations.h"
#define PAGESIZE 4096 
typedef struct {
	uint32_t ppn ;
	uint8_t valid;

} page_table_entry;


typedef struct {
	page_table_entry *entries;
	int max_size_pagetable;
	int curr_size_pagetable;
	uint32_t *ppn_list;
} page_table;
typedef struct {
	uint32_t *freelist;
	int num_free_pages;
	int max_phys_pages;
	

} physical_memory;

typedef struct {
	int pg_hits;
	int pg_free;
	int pg_fault;
} statistics;

void* make_page_table( Results *r , physical_memory *phys_mem, int amountofprocesses); 
#endif //
