DISA=../../FDisassem/src

all : fpemu

fpemu : fpemu.o $(DISA)/fdisa.o
	gcc -O0 -g3 fpemu.o $(DISA)/fdisa.o -o fpemu

fpemu.o : fpemu.c $(DISA)/fdisa.h
	gcc -c -O0 -I$(DISA) -Werror -Wall -Wextra -g3 $< -o $@

tags :
	ctags *.c

clean :
	-rm -f fpemu

# --------------- end of file -----------------------------------------
