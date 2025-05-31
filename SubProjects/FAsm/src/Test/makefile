
FA = ../fa

ASMFILES=$(wildcard *.asm)
HEXFILES=$(ASMFILES:%.asm=%.hex)
SYMFILES=$(ASMFILES:%.asm=%.sym)

%.hex : %.asm
	$(FA) $< -o $@ -l $(@:%.hex=%.list) -s  $(@:%.hex=%.sym)

all : $(HEXFILES)
	echo 'done'

#	$(FA) test_001.asm -o test_001.hex -l test_001.list

clean :
	-rm -f *.list
	-rm -f *.hex
	-rm -f *.sym

# --------------- end of file -----------------------------------------
