#include "virtual_memory.h"
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

// returns allocated array of page tables  
void* make_page_table( Results *r , physical_memory *phys_mem,int amountofprocesses ){ 
	//make pagetable array 
	page_table *pagetables = malloc(amountofprocesses * sizeof(page_table) );
	if (pagetables == NULL){
		fprintf(stderr,"page table memory allocation fail\n" );
		return NULL;
	}
	for (int i =0; i < amountofprocesses;i++){
		pagetables[i].entries  = calloc ( 512 * 1024 , sizeof(page_table_entry));
		if (pagetables[i].entries == NULL) {
            fprintf(stderr, "Failed to allocate page table for process %d\n", i);
            // Cleanup previously allocated entries 
            for (int j = 0; j < i; j++) {
                free(pagetables[j].entries);
            }
            free(pagetables);
            return NULL;
        }
        pagetables[i].max_size_pagetable = 512 * 1024;
        pagetables[i].curr_size_pagetable = 0;

	}

	uint64_t user_pages = r->phys_pages - r->sys_pages;
    phys_mem->num_free_pages = user_pages;
    phys_mem->max_phys_pages = r->phys_pages;
    
    phys_mem->freelist = malloc(user_pages * sizeof(uint32_t));
    if (phys_mem->freelist == NULL) {
        fprintf(stderr, "Failed to allocate free page list\n");
        for (int i = 0; i < amountofprocesses; i++) {
            free(pagetables[i].entries);
        }
        free(pagetables);
        return NULL;
    }
    
    for (uint64_t i = 0; i < user_pages; i++) {
        phys_mem->freelist[i] = i;
    }

	return pagetables;
}

void process_trace(Config *c , Results* r){
	





}

