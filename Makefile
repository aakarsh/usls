all: usls.c
	  gcc -g -Wall -o usls usls.c

clean:
	$(RM) usls

