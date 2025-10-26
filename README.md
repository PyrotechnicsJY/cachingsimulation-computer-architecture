CS-3853-001 Virtual Memory_&_Cache_Simulator

Group Members: 
    - Cameron Bergeron (anl176)
    - Christopher Delgado
    - Chad Hoggard

This project is done in C.

To compile the program, run:

make

This uses the provided Makefile to build an executable named cache_sim

Alternatively, you can compile manually with:

gcc main.c calculations.c -o cache_sim -lm

Example usage:

./cache_sim -s 512 -b 16 -a 4 -r RR -p 1024 -n -1 -u 25 -f trace1.trc

| Flag | Description                                                         |
| ---- | ------------------------------------------------------------------- |
| `-s` | Cache size in KB (8–8192, power of 2)                               |
| `-b` | Block size in bytes (8, 16, 32, 64)                                 |
| `-a` | Associativity (1, 2, 4, 8, 16)                                      |
| `-r` | Replacement policy (`RR` = Round Robin, `RND` = Random)             |
| `-p` | Physical memory size in MB (128–4096, power of 2)                   |
| `-n` | Instructions per time slice (`-1` for All)                          |
| `-u` | Percent of memory used by OS (0–100)                                |
| `-f` | One to three trace file paths (e.g., `-f trace1.trc -f trace2.trc`) |

To remove the compiled binary:
rm cache_sim.exe
