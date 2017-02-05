exor: src/main.c
	gcc -o exor src/main.c `pkg-config vte-2.90 --libs --cflags`
