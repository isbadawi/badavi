CC = clang
CFLAGS = -Wall -g

OBJECTS=buf.o buffer.o gap.o mode.o util.o editor.o window.o motion.o \
  normal_mode.o insert_mode.o command_mode.o operator_pending_mode.o

badavi: main.c $(OBJECTS)
	$(CC) main.c $(OBJECTS) -o badavi $(CFLAGS) -ltermbox

.PHONY: clean
clean:
	rm -f *.o
