badavi: main.c piece.o
	clang main.c piece.o -o badavi -ltermbox

piece.o: piece.c piece.h
	clang -c piece.c

hello: hello.c
	clang hello.c -o hello -ltermbox

clean:
	rm -f *.o
