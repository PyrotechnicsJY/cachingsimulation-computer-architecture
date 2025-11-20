#include "virtual_memory.h"
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

// returns allocated array of page tables  
page_table* make_page_table(int amountofprocesses ){ 
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


	return pagetables;
}

void free_page_tables(page_table *pagetables, int amountofprocesses){
	if (!pagetables) return;

	for (int i = 0; i < amountofprocesses; i++){
		free(pagetables[i].entries);
	}
	free(pagetables);
}

physical_memory* make_physical_memory(Results *r){
	
	//page_table *pagetables = malloc(amountofprocesses * sizeof(page_table) );
	physical_memory *phys_mem = malloc(sizeof(physical_memory));
	if (phys_mem == NULL ){
		fprintf(stderr, "Failed to allocate memory for physical_memory\n");
	}
	uint64_t user_pages = r->phys_pages - r->sys_pages;
    phys_mem->num_free_pages = user_pages;
    phys_mem->max_phys_pages = r->phys_pages;
    
    phys_mem->freelist = malloc(user_pages * sizeof(uint32_t));
    if (phys_mem->freelist == NULL) {
        fprintf(stderr, "Failed to allocate free page list\n");
		free(phys_mem);
        return NULL;
    }
    
    for (uint64_t i = 0; i < user_pages; i++) {
        phys_mem->freelist[i] = r->sys_pages + i;
    }
	return phys_mem;
	
}
void free_physical_memory(physical_memory* phys_mem){
	if (!phys_mem) return;
	free(phys_mem->freelist);
	free(phys_mem);
}
void process_trace(Config *cfg , Results* res){
	// depenends on the length of the files assume 
	const int LENGTH_MAX = 1024;
	
	for( int i = 0; i < cfg->num_traces; i++){
	char line[LENGTH_MAX];
	FILE *currFile = fopen(cfg->traces[i],"r");
	if (currFile == NULL){
		fprintf(stderr, "Error: could not open file %s going to next file in argument\n", cfg->traces[i]);
		continue;
	}
	while (fgets(line,LENGTH_MAX, currFile) != NULL){

	//if (strcmp("\n",line) == 0){
	//	continue;
		// skip empty lines
	//}

	printf("%s",line);
   //process lines 	
	}

	printf("end of %s\n",cfg->traces[i]);
	fclose(currFile);
	

	}
	
	


}


