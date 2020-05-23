CC := gcc
CCFLAGS := -g -I$(HFILES)

SRC := src
BIN := bin
OBJ := obj
TEST := test

CFILES := $(wildcard $(SRC)/*.c)
HFILES := $(wildcard $(SRC)/*.h)
OFILES := $(patsubst $(SRC)/%.c, $(OBJ)/%.o, $(CFILES))
TEST_C := $(wildcard $(TEST)/src/*.c)
TEST_O := $(patsubst $(TEST)/src/%.c, $(TEST)/obj/%.o, $(TEST_C))

all : $(OFILES)
	$(CC) -g -o $(BIN)/example $(OFILES) $(OBJ)/example.o

$(OBJ)/%.o : $(SRC)/%.c
	$(CC) -g -c -o $@ $<
	$(CC) -g -c -o $(OBJ)/example.o example.c

clean:
	rm -f $(OBJ)/*
	rm -f $(BIN)/*

$(TEST)/obj/%.o : $(TEST)/src/%.c
	$(CC) -g -c -o $@ $<

test: $(OFILES) $(TEST_O)
	$(CC) -g -o $(TEST)/bin/run $(OFILES) $(TEST_O)
	./test/bin/run
