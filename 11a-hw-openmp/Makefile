
CC = gcc
CFLAGS = -Wall -O2 -g

all: mandelbrot
.PHONY: all

mandelbrot: mandelbrot.c
	$(CC) $(CFLAGS) -o mandelbrot mandelbrot.c

.PHONY: clean
clean:
	rm -f mandelbrot
