CC = gcc
CFLAGS = -lm -fsanitize=address -Wall -Wextra
SRC = main.c calculations.c cache.c virtual_memory.c
OUT = VMCacheSim

all: $(OUT)

$(OUT): $(SRC)
	$(CC) $(SRC) -o $(OUT) $(CFLAGS)

clean:
	rm -f $(OUT) *.o

