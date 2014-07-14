INCLUDE_FLAGS= -I. -Igen/


all: uls ucat uwc ush

uls: usls.c 
	gcc -g -Wall $(INCLUDE_FLAGS) -o bin/uls usls.c
	cp bin/* ~/bin


uwc: uwc.c 
	gcc -g -Wall $(INCLUDE_FLAGS) -o bin/uwc uwc.c
	cp bin/* ~/bin

uthread: uthread.c 
	gcc -g -Wall $(INCLUDE_FLAGS) -o bin/uthread uthread.c

ucat: ucat.c 
	gcc -g -Wall -o bin/ucat ucat.c

ush: ush_lex.l ush_parser.y 
	bison -o gen/ush_parser.tab.c --defines=gen/ush_tokens.h ush_parser.y   # produce ush_parser.tab.c
	flex -o gen/ush_lex.yy.c ush_lex.l 
	gcc -o obj/ush_lex.o -Igen/ $(INCLUDE_FLAGS) -c gen/ush_lex.yy.c -lfl -ly
	gcc -g -Wall -Lobj -Igen/  $(INCLUDE_FLAGS) -o bin/ush ush.c obj/ush_lex.o -lfl -ly


cp-bin: bin/ucat bin/ush bin/uls bin/ush
	cp bin/* ~/bin

clean:
	$(RM) bin/*
	$(RM) ush_parser.tab.c 
	$(RM) ush_lex.yy.c

