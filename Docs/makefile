
svg_files = $(wildcard *.svg)
png_files = $(svg_files:.svg=.png)
pdf_files = $(svg_files:.svg=.pdf)

%.png : %.svg
	inkscape --export-filename=$@ $< --export-width=1600

%.pdf : %.svg
	inkscape --export-filename=$@ $<

stack-master-16-user-manual.pdf : stack-master-16-user-manual.md
	pandoc --toc --number-sections -o $@ $<

all : $(pdf_files) $(png_files) stack-master-16-user-manual.pdf
	echo "done"

clean :
	-rm -f $(png_files)
	-rm -f $(pdf_files)
	-rm -f stack-master-16-user-manual.pdf

# --------------- end of file -----------------------------------------
