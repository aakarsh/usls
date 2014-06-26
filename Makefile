all: uls ucat ush 

uls: usls.c 
	gcc -g -Wall -o bin/uls usls.c
	cp bin/* ~/bin
ucat: ucat.c 
	gcc -g -Wall -o bin/ucat ucat.c
	cp bin/* ~/bin
ush: ush.c ush-lex
	gcc -g -Wall -o bin/ush ush.c
	cp bin/* ~/bin

ush-lex: ush_lex.l
	flex -o ush_lex.yy.c ush_lex.l 
	gcc -g -Wall ush_lex.yy.c -lfl
#valgrind --leak-check=yes  uls ~  | tee v.outx

clean:
	$(RM) bin/*

