CC = gcc
CFLAGS = -Wall -pthread -Wextra -std=c11 -pedantic
TARGET = hw3

all: $(TARGET)

$(TARGET): main.c
	$(CC) $(CFLAGS) -o $(TARGET) main.c

clean:
	rm -f $(TARGET)

.PHONY: all clean