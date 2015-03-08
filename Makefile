CC = clang
CFLAGS = -Wall -Werror -pedantic -g

OBJECTS=buf.o buffer.o gap.o mode.o util.o editor.o window.o motion.o \
  normal_mode.o insert_mode.o operator_pending_mode.o list.o

badavi: main.c $(OBJECTS)
	$(CC) main.c $(OBJECTS) -o badavi $(CFLAGS) -ltermbox

.PHONY: clean
clean:
	rm -f *.o
