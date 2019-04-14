# # # # # # #
# Makefile for assignment 2
#
# created May 2018
# Matt Farrugia <matt.farrugia@unimelb.edu.au>
#

CC     = gcc
CFLAGS = -Wall -Wpedantic -std=c99
# modify the flags here ^
EXE    = image_tagger
OBJ    = image_tagger.o hashtbl.o list.o strhash.o
# add any new object files here ^

# top (default) target
all: $(EXE)

# how to link executable
$(EXE): $(OBJ)
	$(CC) $(CFLAGS) -o $(EXE) $(OBJ)

# other dependencies, what headers are included by these objects
image_tagger.o: hashtbl.h list.h
hashtbl.o: strhash.h

# ^ add any new dependencies here (for example if you add new modules)


# phony targets (these targets do not represent actual files)
.PHONY: clean cleanly all CLEAN

# `make clean` to remove all object files
# `make CLEAN` to remove all object and executable files
# `make cleanly` to `make` then immediately remove object files (inefficient)
clean:
	rm -f $(OBJ)
CLEAN: clean
	rm -f $(EXE)
cleanly: all clean
