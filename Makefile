CFLAGS?=-Wall -O2
LDADD?=$(shell pkg-config --cflags --libs x11 xinerama x11 xft)

all: normal

normal:
	$(CC) -o goomwwm goomwwm.c $(CFLAGS) $(LDADD) $(LDFLAGS)

debug:
	$(CC) -o goomwwm-debug goomwwm.c $(CFLAGS) -g -DDEBUG $(LDADD)

clean:
	rm -f goomwwm goomwwm-debug
