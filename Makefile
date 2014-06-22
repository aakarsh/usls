all: usls.c
	gcc -g -Wall -o uls usls.c
	cp uls ~/bin
	valgrind --leak-check=yes  uls ~  | tee v.out

clean:
	$(RM) u

