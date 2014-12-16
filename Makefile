badavi: main.c util.o
	clang main.c util.o -o badavi -ltermbox

util.o: util.c util.h
	clang -c util.c

hello: hello.c
	clang hello.c -o hello -ltermbox

clean:
	rm -f *.o
