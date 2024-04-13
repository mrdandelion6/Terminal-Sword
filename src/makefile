# default port number
PORT = 55976

# compiler to use
CC = gcc

# compiler flags
CFLAGS = -DPORT=$(PORT) -g -Wall

# source files
SRC = sword.c

# object files
OBJ = $(SRC:.c=.o)

# executable name
EXEC = sword

.PHONY: all clean
all: $(EXEC)

$(EXEC): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) $(EXEC)

