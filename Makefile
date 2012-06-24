CFLAGS?=-Wall -O2 -g -DDEBUG

all:
	$(CC) $(CFLAGS) -I/usr/include/freetype2 -lX11 -lXinerama -lXft -lXrender -lfreetype -lz -lfontconfig -o goomwwm goomwwm.c

clean:
	rm -f goomwwm
