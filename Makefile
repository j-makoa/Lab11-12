# Makefile
# Name: Jim Luzano-Belfield
# Assignment: Lab 11 - Multiprocessing
# Section: CPE-2600-<SECTION>
#
# Builds two programs:
#   mandel       - renders one Mandelbrot image (provided program)
#   mandelmovie  - forks children that run mandel to render movie frames

CC=gcc
CFLAGS=-c -Wall -g
LDFLAGS=-ljpeg -lm

MANDEL_SOURCES=mandel.c jpegrw.c
MOVIE_SOURCES=mandelmovie.c
SOURCES=$(MANDEL_SOURCES) $(MOVIE_SOURCES)

MANDEL_OBJECTS=$(MANDEL_SOURCES:.c=.o)
MOVIE_OBJECTS=$(MOVIE_SOURCES:.c=.o)
OBJECTS=$(SOURCES:.c=.o)

EXECUTABLES=mandel mandelmovie

all: $(EXECUTABLES)

# pull in dependency info for *existing* .o files
-include $(OBJECTS:.o=.d)

mandel: $(MANDEL_OBJECTS)
	$(CC) $(MANDEL_OBJECTS) $(LDFLAGS) -o $@

mandelmovie: $(MOVIE_OBJECTS)
	$(CC) $(MOVIE_OBJECTS) -lm -o $@

.c.o:
	$(CC) $(CFLAGS) $< -o $@
	$(CC) -MM $< > $*.d

# make the movie from the frames (run ./mandelmovie first)
movie:
	ffmpeg -y -framerate 10 -i mandel%d.jpg -r 25 -q:v 2 mandel.mpg

clean:
	rm -rf $(OBJECTS) $(EXECUTABLES) *.d

.PHONY: all clean movie
