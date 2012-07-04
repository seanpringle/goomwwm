CFLAGS?=-Wall -O2
LDADD?=$(shell pkg-config --cflags --libs x11 xinerama x11 xft)

all: normal

normal:
	$(CC) $(CFLAGS) $(LDADD) $(LDFLAGS) -o goomwwm goomwwm.c

debug:
	$(CC) $(CFLAGS) -g -DDEBUG $(LDADD) -o goomwwm-debug goomwwm.c

clean:
	rm -f goomwwm goomwwm-debug
