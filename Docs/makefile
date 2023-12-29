
svg_files = $(wildcard *.svg)
png_files = $(svg_files:.svg=.png)
pdf_files = $(svg_files:.svg=.pdf)

%.png : %.svg
	inkscape --export-filename=$@ $< --export-width=1600

%.pdf : %.svg
	inkscape --export-filename=$@ $<

all : $(pdf_files) $(png_files)
	echo "done"

clean :
	-rm -f $(png_files)
	-rm -f $(pdf_files)

