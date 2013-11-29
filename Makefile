CFLAGS = -std=c99

gc : gc.c
	gcc -o $@ $< $(CFLAGS)