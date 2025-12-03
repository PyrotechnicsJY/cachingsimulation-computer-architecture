#include "virtual_memory.h"

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

page_table *make_page_table(int amountofprocesses) {
    page_table *pagetables = malloc(amountofprocesses * sizeof(page_table));
    if (!pagetables) {
        fprintf(stderr, "page table memory allocation fail\n");
        return NULL;
    }

    for (int i = 0; i < amountofprocesses; i++) {
        pagetables[i].entries = calloc(NUM_VPAGES, sizeof(page_table_entry));
        if (!pagetables[i].entries) {
            fprintf(stderr, "Failed to allocate page table for process %d\n", i);
            for (int j = 0; j < i; j++) {
                free(pagetables[j].entries);
            }
            free(pagetables);
            return NULL;
        }
        pagetables[i].max_size_pagetable = NUM_VPAGES;
        pagetables[i].curr_size_pagetable = 0;
    }

    return pagetables;
}

void free_page_tables(page_table *pagetables, int amountofprocesses) {
    if (!pagetables) return;
    for (int i = 0; i < amountofprocesses; i++) {
        free(pagetables[i].entries);
    }
    free(pagetables);
}

physical_memory *make_physical_memory(Results *r) {
    physical_memory *phys_mem = malloc(sizeof(physical_memory));
    if (!phys_mem) {
        fprintf(stderr, "Failed to allocate memory for physical_memory\n");
        return NULL;
    }

    uint64_t user_pages = r->phys_pages - r->sys_pages;
    phys_mem->num_free_pages = (int)user_pages;
    phys_mem->max_phys_pages = (int)r->phys_pages;

    phys_mem->freelist = malloc(user_pages * sizeof(uint32_t));
    if (!phys_mem->freelist) {
        fprintf(stderr, "Failed to allocate free page list\n");
        free(phys_mem);
        return NULL;
    }

    // Fill free list with user page numbers [sys_pages .. phys_pages-1]
    for (uint64_t i = 0; i < user_pages; i++) {
        phys_mem->freelist[i] = (uint32_t)(r->sys_pages + i);
    }

    return phys_mem;
}

void free_physical_memory(physical_memory *phys_mem) {
    if (!phys_mem) return;
    free(phys_mem->freelist);
    free(phys_mem);
}

static uint32_t handle_page_fault(int pid,
                                  uint32_t faulting_vpn,
                                  page_table *pt_array,
                                  statistics *stats) {
    // No free pages left: choose a victim page from this process
    stats[pid].pg_fault++;

    page_table *pt = &pt_array[pid];
    uint32_t victim_vpn = 0;
    uint32_t victim_ppn = 0;
    int found = 0;

    for (uint32_t vp = 0; vp < (uint32_t)pt->max_size_pagetable; vp++) {
        if (pt->entries[vp].valid) {
            victim_vpn = vp;
            victim_ppn = pt->entries[vp].ppn;
            found = 1;
            break;
        }
    }

    if (!found) {
        // Should not happen, but avoid UB
        return 0;
    }

    // Unmap victim
    pt->entries[victim_vpn].valid = 0;
    if (pt->curr_size_pagetable > 0) {
        pt->curr_size_pagetable--;
    }

    // Map new page
    page_table_entry *e = &pt->entries[faulting_vpn];
    if (!e->valid) {
        pt->curr_size_pagetable++;
    }
    e->ppn = victim_ppn;
    e->valid = 1;

    return victim_ppn;
}

uint32_t vm_translate_addr(int pid,
                           uint32_t vaddr,
                           page_table *pt_array,
                           physical_memory *phys_mem,
                           statistics *stats) {
    uint32_t vpn = vaddr >> 12;  // 4KB pages
    page_table *pt = &pt_array[pid];

    if (vpn >= (uint32_t)pt->max_size_pagetable) {
        fprintf(stderr, "vm_translate_addr: VPN %u out of range for pid %d\n",
                vpn, pid);
        return 0;
    }

    page_table_entry *e = &pt->entries[vpn];

    if (e->valid) {
        stats[pid].pg_hits++;
        return e->ppn;
    }

    // If we have free physical pages, allocate one
    if (phys_mem->num_free_pages > 0) {
        phys_mem->num_free_pages--;
        uint32_t ppn = phys_mem->freelist[phys_mem->num_free_pages];

        stats[pid].pg_free++;

        e->ppn = ppn;
        e->valid = 1;
        pt->curr_size_pagetable++;

        return ppn;
    }

    // Otherwise we must page fault and steal a page
    return handle_page_fault(pid, vpn, pt_array, stats);
}

void process_trace_vm(Config *cfg,
                      Results *res,
                      page_table *pt_array,
                      physical_memory *phys_mem,
                      statistics *stats_per_proc,
                      Cache *cache,
                      CacheStats *cstats) {
    (void)res; // currently unused, but kept for future extensions

    const int LENGTH_MAX = 1024;

    for (int pid = 0; pid < cfg->num_traces; pid++) {
        char line1[LENGTH_MAX];
        char line2[LENGTH_MAX];

        FILE *f = fopen(cfg->traces[pid], "r");
        if (!f) {
            fprintf(stderr, "Error: could not open file %s\n", cfg->traces[pid]);
            continue;
        }

        long long instr_count = 0;

        while (fgets(line1, LENGTH_MAX, f)) {
            unsigned int ilen = 0;
            unsigned int eip_addr = 0;

            // Parse instruction line: "EIP (len): addr ..."
            if (sscanf(line1, " EIP (%u): %x", &ilen, &eip_addr) == 2) {
                // Translate instruction virtual address to physical page
                int prev_faults = stats_per_proc[pid].pg_fault;
                uint32_t ppn = vm_translate_addr(pid,
                                                 (uint32_t)eip_addr,
                                                 pt_array,
                                                 phys_mem,
                                                 stats_per_proc);
                if (stats_per_proc[pid].pg_fault > prev_faults && cache) {
                    // On a page fault, conservatively flush the cache
                    cache_flush(cache);
                }

                uint32_t offset = (uint32_t)eip_addr & (PAGESIZE - 1u);
                paddr_t  paddr  = (ppn << 12) | offset;

                // Simulate cache access for instruction fetch
                cache_access(cache, cstats, paddr, ilen);
                cstats->total_cycles += 2;    // +2 cycles for instruction execution
                cstats->instr_bytes  += ilen;
                cstats->instr_count  += 1;

                instr_count++;
                if (cfg->time_slice != -1 && instr_count >= cfg->time_slice) {
                    // Still need to consume the paired data line to stay in sync
                    if (!fgets(line2, LENGTH_MAX, f)) {
                        break;
                    }
                    break;
                }

                // Read the data access line corresponding to this instruction
                if (!fgets(line2, LENGTH_MAX, f)) {
                    break;
                }

                unsigned int dst_addr = 0, src_addr = 0;
                char dst_data[9] = {0}, src_data[9] = {0};

                int n = sscanf(line2,
                               " dstM: %x %8s srcM: %x %8s",
                               &dst_addr, dst_data,
                               &src_addr, src_data);

                if (n == 4) {
                    // Destination write (4 bytes) if address and data valid
                    if (dst_addr != 0 && strcmp(dst_data, "--------") != 0) {
                        int prev_faults_dst = stats_per_proc[pid].pg_fault;
                        uint32_t dst_ppn = vm_translate_addr(pid,
                                                             (uint32_t)dst_addr,
                                                             pt_array,
                                                             phys_mem,
                                                             stats_per_proc);
                        if (stats_per_proc[pid].pg_fault > prev_faults_dst && cache) {
                            cache_flush(cache);
                        }

                        uint32_t dst_off = (uint32_t)dst_addr & (PAGESIZE - 1u);
                        paddr_t  dst_paddr = (dst_ppn << 12) | dst_off;

                        cache_access(cache, cstats, dst_paddr, 4u);
                        cstats->total_cycles += 1;  // +1 cycle for effective address
                        cstats->srcdst_bytes += 4u;
                    }

                    // Source read (4 bytes) if address and data valid
                    if (src_addr != 0 && strcmp(src_data, "--------") != 0) {
                        int prev_faults_src = stats_per_proc[pid].pg_fault;
                        uint32_t src_ppn = vm_translate_addr(pid,
                                                             (uint32_t)src_addr,
                                                             pt_array,
                                                             phys_mem,
                                                             stats_per_proc);
                        if (stats_per_proc[pid].pg_fault > prev_faults_src && cache) {
                            cache_flush(cache);
                        }

                        uint32_t src_off = (uint32_t)src_addr & (PAGESIZE - 1u);
                        paddr_t  src_paddr = (src_ppn << 12) | src_off;

                        cache_access(cache, cstats, src_paddr, 4u);
                        cstats->total_cycles += 1;  // +1 cycle for effective address
                        cstats->srcdst_bytes += 4u;
                    }
                }
            }
        }

        fclose(f);
    }
}

void print_vm_results(Config *cfg,
                      Results *res,
                      page_table *pt_array,
                      statistics *stats_per_proc) {
    uint64_t phys_pages = res->phys_pages;
    uint64_t sys_pages  = res->sys_pages;
    uint64_t user_pages = phys_pages - sys_pages;

    uint64_t total_mapped  = 0;
    uint64_t total_hits    = 0;
    uint64_t total_free    = 0;
    uint64_t total_faults  = 0;

    for (int i = 0; i < cfg->num_traces; i++) {
        total_mapped += (uint64_t)pt_array[i].curr_size_pagetable;
        total_hits   += (uint64_t)stats_per_proc[i].pg_hits;
        total_free   += (uint64_t)stats_per_proc[i].pg_free;
        total_faults += (uint64_t)stats_per_proc[i].pg_fault;
    }

    //MILESTONE #2: - Virtual Memory Simulation Results

    printf("***** VIRTUAL MEMORY SIMULATION RESULTS *****\n\n");
    printf("Physical Pages Used By SYSTEM:   %llu\n",
           (unsigned long long)sys_pages);
    printf("Pages Available to User:         %llu\n\n",
           (unsigned long long)user_pages);
    printf("Virtual Pages Mapped:            %llu\n",
           (unsigned long long)total_mapped);
    printf("        ------------------------------\n");
    printf("        Page Table Hits: %llu\n\n",
           (unsigned long long)total_hits);
    printf("        Pages from Free: %llu\n\n",
           (unsigned long long)total_free);
    printf("        Total Page Faults: %llu\n\n",
           (unsigned long long)total_faults);

    printf("Page Table Usage Per Process:\n");
    printf("------------------------------\n");

    for (int i = 0; i < cfg->num_traces; i++) {
        int used = pt_array[i].curr_size_pagetable;
        double percent = 0.0;
        if (NUM_VPAGES > 0) {
            percent = (100.0 * (double)used) / (double)NUM_VPAGES;
        }

        uint64_t wasted_bytes = 0;
        if (res->pte_bits > 0) {
            uint64_t unused_entries = (uint64_t)NUM_VPAGES - (uint64_t)used;
            wasted_bytes = unused_entries * (uint64_t)res->pte_bits / 8ULL;
        }

        printf("[%d] %s:\n", i, cfg->traces[i]);
        printf("       Used Page Table Entries: %d ( %.2f%%)\n",
               used, percent);
        printf("       Page Table Wasted: %llu bytes\n",
               (unsigned long long)wasted_bytes);
    }
}
