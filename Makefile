all: usls.c ucat.c
	gcc -g -Wall -o uls usls.c
	gcc -g -Wall -o ucat ucat.c
	cp uls ~/bin
	cp ucat ~/bin
	#valgrind --leak-check=yes  uls ~  | tee v.out


clean:
	$(RM) u

