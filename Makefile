all: usls.c
	gcc -g -Wall -o usls usls.c
	global -u

clean:
	$(RM) usls

