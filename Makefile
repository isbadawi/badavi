CC = clang
CFLAGS = -Wall -g

OBJECTS=buf.o editor.o mode.o file.o util.o

badavi: main.c $(OBJECTS)
	$(CC) main.c $(OBJECTS) -o badavi $(CFLAGS) -ltermbox

.PHONY: clean
clean:
	rm -f *.o
