NAME=math
SRC=$(NAME).cc
WASM=$(NAME).wasm
HEADER=$(NAME).wasm.h
EMCC_FLAGS=-O3 -Wl,--no-entry -Wl,--export=apply -Wl,--allow-undefined \
			-s STANDALONE_WASM -s ERROR_ON_UNDEFINED_SYMBOLS=0 \
			-std=c++17 \
			-I ./ \
			-I ../ \
			-I ../../vm/ \
			-I ../../contract/ \
			-I ../../libraries	\
			-I ../../libraries/boost/include	\
			-v

all: $(HEADER)

clean:
	rm -f $(HEADER) $(WASM)

.PHONY: all clean

$(WASM): $(SRC)
	emcc $< -o $@ $(EMCC_FLAGS)

$(HEADER): $(WASM)
	xxd -i $< >$@
