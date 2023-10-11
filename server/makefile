#################### MAKEFILE #################


# Compiler
CC := $(CROSS_COMPILE)gcc 
CFLAGS := -g -Wall -Werror
EXEC = aesdsocket


# Target
all: $(EXEC)
default: $(EXEC)

aesdsocket: aesdsocket.o
	$(CC) $(CFLAGS) -o aesdsocket aesdsocket.o

aesdsocket.o: aesdsocket.c
	$(CC) $(CFLAGS) -c aesdsocket.c


clean:
	rm -f *.o $(EXEC)
    
