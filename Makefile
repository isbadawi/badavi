CC = clang
CFLAGS = -Wall -g

OBJECTS=buf.o editor.o normal_mode.o insert_mode.o command_mode.o file.o util.o

badavi: main.c $(OBJECTS)
	$(CC) main.c $(OBJECTS) -o badavi $(CFLAGS) -ltermbox

.PHONY: clean
clean:
	rm -f *.o
