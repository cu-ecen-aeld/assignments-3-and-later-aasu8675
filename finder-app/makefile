# Compiler and flags
CC := $(if $(CROSS_COMPILE), $(CROSS_COMPILE)gcc, gcc) 
CFLAGS := -Wall -Werror 

SRC := writer.c
OBJ := $(SRC:.c=.o)

# Output Binary
TARGET := writer

.PHONY: all clean

# Default Target
all: $(TARGET)

${TARGET}: $(OBJ)
		$(CC) $(CFLAGS) -o $@ $^

%.o: %.c
		$(CC) $(CFLAGS) -c -o $@ $< 

clean:
		rm -f $(TARGET) $(OBJ)

