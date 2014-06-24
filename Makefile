all: usls.c ucat.c
	gcc -g -Wall -o bin/uls usls.c
	gcc -g -Wall -o bin/ucat ucat.c
	gcc -g -Wall -o bin/uwc uwc.c
	cp bin/* ~/bin

#valgrind --leak-check=yes  uls ~  | tee v.outx

clean:
	$(RM) bin/*

