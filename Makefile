CC = clang
CFLAGS = -Wall -g

OBJECTS=piece.o util.o

badavi: main.c $(OBJECTS)
	$(CC) main.c $(OBJECTS) -o badavi $(CFLAGS) -ltermbox

.PHONY: clean
clean:
	rm -f *.o
