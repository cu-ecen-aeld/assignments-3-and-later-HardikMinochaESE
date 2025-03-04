CROSS_COMPILE ?=
CC ?= (CROSS_COMPILE)gcc
CFLAGS ?= -Wall -Wextra -g
TARGET ?= aesdsocket
LDFLAGS ?= -lpthread -lrt

all: $(TARGET)

$(TARGET): aesdsocket.o
	$(CC) -o $(TARGET) aesdsocket.o $(LDFLAGS)

aesdsocket.o: aesdsocket.c
	$(CC) $(CFLAGS) -c aesdsocket.c

clean:
	rm -f $(TARGET) *.o
	
default: $(TARGET)
