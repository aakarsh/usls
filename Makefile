all: usls.c
	gcc -g -Wall -o u usls.c
	global -u

clean:
	$(RM) u

