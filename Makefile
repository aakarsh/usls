all: usls.c ucat.c
	gcc -g -Wall -o uls usls.c
	gcc -g -Wall -o ucat ucat.c
	gcc -g -Wall -o uwc uwc.c
	cp uls ~/bin
	cp ucat ~/bin
	cp uwc ~/bin
#valgrind --leak-check=yes  uls ~  | tee v.outx

clean:
	$(RM) u

