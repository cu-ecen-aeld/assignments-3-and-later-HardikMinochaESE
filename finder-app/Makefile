CC=$(CROSS_COMPILE)gcc
CFLAGS=-Wall -Werror

all: writer

writer: writer.c
	rm -f writer
	$(CC) $(CFLAGS) -o writer writer.c
	echo $?
	
	
clean:
	rm -f writer

