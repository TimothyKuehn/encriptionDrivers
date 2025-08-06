# Compiler and flags
CC = gcc
CFLAGS = -lpthread

# Target executable
TARGET = encrypt

# Source files
SRC = encrypt-driver.c encrypt-module.c

# Default target
all: $(TARGET)

# Rule to build the target
$(TARGET): $(SRC)
	$(CC) $(SRC) $(CFLAGS) -o $(TARGET)

# Clean up build artifacts
clean:
	rm -f $(TARGET)