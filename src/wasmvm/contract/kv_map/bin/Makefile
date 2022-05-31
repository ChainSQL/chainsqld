NAME=main
SRC=$(NAME).cc
BIN=main
FLAGS=-O3 -std=c++17 \
			-I ./ \
			-I ../../../libraries \
			-I ../../../libraries/meta_refl/include \
			-I ../../../libraries/boost/include

all: $(BIN)

clean:
	rm -f $(HEADER) $(BIN)

.PHONY: all clean

$(BIN): $(SRC)
	g++ $< -o $@ $(FLAGS)
