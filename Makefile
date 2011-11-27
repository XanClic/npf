CC = gcc
CFLAGS = -Wall -Wextra -pedantic -std=c1x -O3

RM = rm -f

FILES = $(patsubst %.c,%,$(wildcard *.c))

.PHONY: all clean

all: $(FILES)

%: %.c
	$(CC) $(CFLAGS) $< -o $@

edit: edit.c
	$(CC) $(CFLAGS) $< -o $@ -lreadline

clean:
	$(RM) $(FILES)
