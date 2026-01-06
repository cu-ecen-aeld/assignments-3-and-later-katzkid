# CROSS_COMPILE = aarch64-none-linux-gnu-
CROSS_COMPILE = 

all: writer

writer : writer.o
	$(CROSS_COMPILE)gcc writer.o -o writer

writer.o : writer.c
	$(CROSS_COMPILE)gcc -c writer.c -Wall -o writer.o

clean:
	rm -rf writer.o writer

