# Default port number
PORT = 53456

# Compiler to use
CC = gcc

# Compiler flags
CFLAGS = -DPORT=$(PORT) -g -Wall

# Source files
SRC = battle.c

# Object files
OBJ = $(SRC:.c=.o)

# Executable name
EXEC = battle

.PHONY: all clean

all: $(EXEC)

$(EXEC): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) $(EXEC)

