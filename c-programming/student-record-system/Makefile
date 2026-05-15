CC = gcc
CFLAGS = -Wall -Wextra -std=c99
TARGET = main.exe
SRCS = main.c student.c file_io.c menu.c utils.c user.c
OBJS = $(SRCS:.c=.o)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: clean
