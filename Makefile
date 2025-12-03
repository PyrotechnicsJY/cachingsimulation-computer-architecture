CC = gcc

# Enable ANSI stdio on MinGW so %llu / %lld work correctly
CFLAGS = -D__USE_MINGW_ANSI_STDIO -Wall -Wextra

SRC = main.c calculations.c cache.c virtual_memory.c
OUT = VMCacheSim

all: $(OUT)

$(OUT): $(SRC)
	$(CC) $(CFLAGS) $(SRC) -o $(OUT) -lm

clean:
	rm -f $(OUT) *.o
