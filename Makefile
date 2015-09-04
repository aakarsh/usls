CFLAGS=-std=gnu99  -D_GNU_SOURCE
INCLUDE_FLAGS= -I. -Igen/


all: uls ucat uwc ush scat

uls: usls.c 
	gcc $(CFLAGS) -g -Wall $(INCLUDE_FLAGS) -o bin/uls usls.c
	cp bin/* ~/bin
	global -u

scat: scatter.c  queue.h queue.c
	gcc $(CFLAGS)  -g -Wall $(INCLUDE_FLAGS)  -o bin/scat queue.c queue.h scatter.c  -pthread -lm
	cp bin/* ~/bin
	global -u

vscat: scat
	valgrind --tool=memcheck --leak-check=yes --show-reachable=yes --num-callers=20 --track-fds=yes ./bin/scat Eggert - <./bin/emacs.out

uwc: uwc.c 
	gcc $(CFLAGS) -g -Wall $(INCLUDE_FLAGS) -o bin/uwc uwc.c
	cp bin/* ~/bin
	global -u

uthread: uthread.c 
	gcc $(CFLAGS) -g -Wall $(INCLUDE_FLAGS) -o bin/uthread uthread.c
	cp bin/* ~/bin
	global -u

ucat: ucat.c 
	gcc $(CFLAGS) -g -Wall -o bin/ucat ucat.c
	cp bin/* ~/bin	
	global -u

ush: ush_lex.l ush_parser.y 
	bison -o gen/ush_parser.tab.c --defines=gen/ush_tokens.h ush_parser.y   # produce ush_parser.tab.c
	flex -o gen/ush_lex.yy.c ush_lex.l 
	gcc $(CFLAGS) -o obj/ush_lex.o -Igen/ $(INCLUDE_FLAGS) -c gen/ush_lex.yy.c -lfl -ly
	gcc $(CFLAGS) -g -Wall -Lobj -Igen/  $(INCLUDE_FLAGS) -o bin/ush ush.c obj/ush_lex.o -lfl -ly
	cp bin/* ~/bin
	global -u

cp-bin: bin/ucat bin/ush bin/uls bin/ush
	cp bin/* ~/bin
	global -u

clean:
	$(RM) bin/*
	$(RM) ush_parser.tab.c 
	$(RM) ush_lex.yy.c

