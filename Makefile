all: swm

swm:
	gcc -I/usr/include/X11 src/main.c -L/usr/lib -lX11 -o swm

clean:
	rm -rf swm