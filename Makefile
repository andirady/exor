exor: src/main.c
	gcc -o exor src/main.c -Isrc `pkg-config vte-2.90 --libs --cflags` -Wno-deprecated -Wno-deprecated-declarations
