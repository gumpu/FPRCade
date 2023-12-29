#

HEADERS=$(wildcard *.h)
SOURCES=$(wildcard *.c)

utest_sources = utest.c fcore.c instructions.c decompile.c
ff_sources = ff.c fcore.c instructions.c decompile.c
utest_objects = $(utest_sources:.c=.o)
ff_objects = $(ff_sources:.c=.o)

# Memory check
# CFLAGS=-O0 -g -fsanitize=address -Wall
# Debug
CFLAGS=-O0 -g3 -Wall -std=gnu99 -Wpedantic -fmax-errors=1

%.o : %.c
	gcc $(CFLAGS) -c $< -o $@

all : ff utest

ff : $(ff_objects)
	gcc -Wall $(ff_objects) -o $@

utest : $(utest_objects)
	gcc -Wall $(utest_objects) -o $@

options :
	@echo cfc build options:
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
