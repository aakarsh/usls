all: usls.c
	gcc -g -Wall -o uls usls.c
	cp uls ~/bin
clean:
	$(RM) u

