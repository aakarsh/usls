all: uls ucat ush 

uls: usls.c 
	gcc -g -Wall -o bin/uls usls.c
	cp bin/* ~/bin
ucat: ucat.c 
	gcc -g -Wall -o bin/ucat ucat.c
	cp bin/* ~/bin
ush: ush.c 
	gcc -g -Wall -o bin/ush ush.c
	cp bin/* ~/bin



#valgrind --leak-check=yes  uls ~  | tee v.outx

clean:
	$(RM) bin/*

