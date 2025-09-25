#
#

HEADERS=$(wildcard *.h)
SOURCES=$(wildcard *.c)

ff_sources = ff.c interpret.c library.c
ff_objects = $(ff_sources:.c=.o)

# Memory check
# CFLAGS=-O0 -g -fsanitize=address -Wall
# Debug
CFLAGS=-O0 -g3 -gdwarf-4 -Wall -std=gnu99 -fmax-errors=1 -Wno-unused-function

%.o : %.c
	gcc $(CFLAGS) -c $< -o $@

ff : $(ff_objects)
	gcc -g3 $(ff_objects) -o ff

library.c : library.fs to_c.py
	python to_c.py library.fs > library.c

interpret.c : interpret.fs to_c.py
	python to_c.py interpret.fs > interpret.c

all : ff

options :
	@echo ff build options:
	@echo "CFLAGS  = $(CFLAGS)"
	@echo "LDFLAGS = $(LDFLAGS)"
	@echo "CC      = $(CC)"

clean :
	-rm -f ff
	-rm -f *.o
	-rm -f *.orig
	-rm -f tags
	-rm -f interpret.c  # Generated

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
