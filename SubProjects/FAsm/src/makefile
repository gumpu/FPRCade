all : fa

# Memory check
# CFLAGS=-O0 -g -fsanitize=address -Wall
# Debug
CFLAGS=-O0 -g3 -Wall -std=c99 -Wpedantic -fmax-errors=1

fa : fa.c
	gcc $(CFLAGS) fa.c -o fa

test :
	make -C Test

clean :
	-rm -f fa
	make -C Test clean

# --------------- end of file -----------------------------------------
