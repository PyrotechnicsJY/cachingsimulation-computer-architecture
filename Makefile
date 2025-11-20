VMCacheSim: main.c calculations.c calculations.h cache.c cache.h virtual_memory.c virtual_memory.h
	gcc main.c calculations.c cache.c virtual_memory.c -o VMCacheSim -lm -fsanitize=address
