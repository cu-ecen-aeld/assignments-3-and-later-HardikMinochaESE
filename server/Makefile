CC = gcc
CFLAGS = -Wall -Wextra -g
TARGET = aesdsocket
CROSS_COMPILE=

all: $(TARGET)

$(TARGET): aesdsocket.o
	$(CROSS_COMPILE)$(CC) -o $(TARGET) aesdsocket.o

aesdsocket.o: aesdsocket.c
	$(CROSS_COMPILE)$(CC) $(CFLAGS) -c aesdsocket.c

clean:
	rm -f $(TARGET) *.o
	
default: $(TARGET)
