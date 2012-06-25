CFLAGS?=-Wall -O2
LDADD?=-I/usr/include/freetype2 -lX11 -lXinerama -lXft

all: normal

normal:
	$(CC) $(CFLAGS) $(LDADD) -o goomwwm goomwwm.c

debug:
	$(CC) $(CFLAGS) -g -DDEBUG $(LDADD) -o goomwwm-debug goomwwm.c

clean:
	rm -f goomwwm goomwwm-debug
