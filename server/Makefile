CC = gcc
CFLAGS = -Wall -Wextra -pthread
TARGET = server
SRCS = server.c auth.c

all: $(TARGET)

$(TARGET): $(SRCS) protocol.h auth.h
	$(CC) $(CFLAGS) -o $(TARGET) $(SRCS)

clean:
	rm -f $(TARGET)

.PHONY: all clean
