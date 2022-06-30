CC = clang
CLIBS = -lncurses
CFLAGS = -Wall

all: main.c
	$(CC) -o snake $(CFLAGS) $(CLIBS) main.c
