all :
	echo Done
	make -C SubProjects

clean :
	make -C SubProjects clean
	make -C Docs clean

# --------------- end of file -----------------------------------------
