CC=gcc
CFLAGS=-O2
LDFLAGS=-ldl -lm

all: lua example

lua: lua.c ../minilua.h
	$(CC) $(CFLAGS) -o lua lua.c $(LDFLAGS)

example: example.c ../minilua.h
	$(CC) $(CFLAGS) -o example example.c $(LDFLAGS)

test: all
	./example
	./lua -e "print 'hello world'"

clean:
	rm -f lua example *.exe

