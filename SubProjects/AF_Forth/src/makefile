
AS=../../FAsm/src/fa

all : forth.hex

forth.hex : forth.asm
	$(AS) $< -o $@ -s forth.sym -l forth.list

clean :
	-rm -f forth.hex

# --------------- end of file -----------------------------------------
