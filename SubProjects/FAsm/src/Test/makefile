
FA = ../fa

ASMFILES=$(wildcard *.asm)
HEXFILES=$(ASMFILES:%.asm=%.hex)

%.hex : %.asm
	$(FA) $< -o $@ -l $(@:%.hex=%.list)

all : $(HEXFILES)
	echo 'done'

#	$(FA) test_001.asm -o test_001.hex -l test_001.list

clean :
	-rm -f *.list
	-rm -f *.hex

# --------------- end of file -----------------------------------------
