CC ?= gcc
CFLAGS ?= -Wall -Wextra -O2 -I.
LDFLAGS = 

.PHONY: all clean

all: libmiolk.a example

# Build static library
libmiolk.a: miolk.o
	ar rcs libmiolk.a miolk.o

miolk.o: miolk.c miolk.h lidrm.h
	$(CC) $(CFLAGS) -c miolk.c -o miolk.o

example: example.c libmiolk.a
	$(CC) $(CFLAGS) example.c -L. -lmiolk -o example

clean:
	rm -f *.o *.a example
