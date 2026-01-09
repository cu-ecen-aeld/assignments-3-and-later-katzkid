CROSS_COMPILE =

all: aesdsocket

aesdsocket: aesdsocket.o
	$(CROSS_COMPILE)gcc -o aesdsocket aesdsocket.o

aesdsocket.o : aesdsocket.c
	$(CROSS_COMPILE)gcc -c aesdsocket.c -Wall -Werror -o aesdsocket.o

clean:
	rm -rf aesdsocket aesdsocket.o

default: all