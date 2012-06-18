CFLAGS?=-Wall -O2 -g -DDEBUG

all:
	$(CC) $(CFLAGS) -lX11 -lXinerama -o goomwwm goomwwm.c

clean:
	rm -f goomwwm
