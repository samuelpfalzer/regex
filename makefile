CC := gcc
CCFLAGS := -g -I$(HFILES)

SRC := src
BIN := bin
OBJ := obj

CFILES := $(wildcard $(SRC)/*.c) example.c
HFILES := $(wildcard $(SRC)/*.h)
OFILES := $(patsubst $(SRC)/%.c, $(OBJ)/%.o, $(CFILES))


all : $(OFILES)
	$(CC) -g -o $(BIN)/example $(OFILES)

$(OBJ)/%.o : $(SRC)/%.c
	$(CC) -g -c -o $@ $<

clean:
	rm -f $(OBJ)/*
	rm -f $(BIN)/*

run:
	./$(BIN)/example
