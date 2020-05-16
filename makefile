CC := gcc
CCFLAGS := -I$(HFILES)

SRC := src
BIN := bin
OBJ := obj

CFILES := $(wildcard $(SRC)/*.c)
HFILES := $(wildcard $(SRC)/*.h)
OFILES := $(patsubst $(SRC)/%.c, $(OBJ)/%.o, $(CFILES))


all : $(OFILES)
	$(CC) -o $(BIN)/main $(OFILES)

$(OBJ)/%.o : $(SRC)/%.c
	$(CC) -c -o $@ $<

clean:
	rm -f $(OBJ)/*
	rm -f $(BIN)/*

run:
	./$(BIN)/main