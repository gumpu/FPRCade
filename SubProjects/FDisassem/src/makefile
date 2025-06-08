all : disa_test

disa_test : main.o fdisa.o
	gcc -g3 main.o fdisa.o -o $@

main.o : main.c fdisa.h
	gcc -c -O0 -Werror -Wall -Wextra -g3 $< -o $@

fdisa.o : fdisa.c fdisa.h
	gcc -c -O0 -Werror -Wall -Wextra -g3 $< -o $@

clean :
	-rm -f fdisa.o main.o disa_test

# --------------- end of file -----------------------------------------
