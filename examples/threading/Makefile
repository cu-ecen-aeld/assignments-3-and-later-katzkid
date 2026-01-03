CROSS_COMPILE =

threading : threading.o
	$(CROSS_COMPILE)gcc threading.o -o threading

threading.o : threading.c
	$(CROSS_COMPILE)gcc -c threading.c -Wall -o threading.o
clean:
	rm -rf threading.o threading