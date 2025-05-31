
AS=../../FAsm/src/fa

all : forth.hex

forth.hex : forth.asm
	$(AS) $< -o $@

clean :
	-rm -f forth.hex

# --------------- end of file -----------------------------------------
