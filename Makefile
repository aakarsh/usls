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

ush-lex: ush_lex.l ush_parser.y
	bison --defines=ush_tokens.h ush_parser.y
	flex -o ush_lex.yy.c ush_lex.l 
	gcc -o obj/ush_lex.o -c ush_lex.yy.c -lfl -ly
	gcc -g -Wall -Lobj  -I. -o bin/par ush_parser.tab.c obj/ush_lex.o  -lfl  -ly

clean:
	$(RM) bin/*

