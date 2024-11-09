all : fa

# Memory check
# CFLAGS=-O4 -g -fsanitize=address -Wall

# Debug
CFLAGS=-O0 -g3 -Wall -std=c17 -Wpedantic -fmax-errors=1 -std=c17

fa : fa.c
	gcc $(CFLAGS) fa.c -o fa

test :
	make -C Test

clean :
	-rm -f fa
	make -C Test clean

# --------------- end of file -----------------------------------------
