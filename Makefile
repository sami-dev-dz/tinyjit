CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -Iinclude -g
# Pour activer libgccjit à l'étape JIT :
# LDFLAGS = -lgccjit

SRC = $(wildcard src/*.c)
OBJ = $(SRC:src/%.c=build/%.o)
TARGET = build/my_jit_vm

all: $(TARGET)

$(TARGET): $(OBJ)
	@mkdir -p build
	$(CC) $(OBJ) -o $@ $(LDFLAGS)

build/%.o: src/%.c
	@mkdir -p build
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf build/