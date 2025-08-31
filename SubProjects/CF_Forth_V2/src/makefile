#
#

HEADERS=$(wildcard *.h)
SOURCES=$(wildcard *.c)

ff_sources = ff.c
ff_objects = $(ff_sources:.c=.o)

# Memory check
# CFLAGS=-O0 -g -fsanitize=address -Wall
# Debug
CFLAGS=-O0 -g3 -gdwarf-4 -Wall -std=gnu99 -Wpedantic -fmax-errors=1 -Wno-unused-function

%.o : %.c
	gcc $(CFLAGS) -c $< -o $@

ff : $(ff_objects)
	gcc -g3 $(ff_objects) -o ff

all : ff

options :
	@echo ff build options:
	@echo "CFLAGS  = $(CFLAGS)"
	@echo "LDFLAGS = $(LDFLAGS)"
	@echo "CC      = $(CC)"

clean :
	-rm -f ff utest
	-rm -f *.o
	-rm -f *.orig

stats :
	cloc .

depend: .depend

.depend: $(SOURCES) $(HEADERS)
	rm -f "$@"
	gcc $(CFLAGS) -MM $(SOURCES) $(HEADERS) > .depend

include .depend

tags : ff.c
	ctags *.c *.h

# --------------- end of file -----------------------------------------
