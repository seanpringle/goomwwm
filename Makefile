CFLAGS?=-Wall -O2
LDADD?=$(shell pkg-config --cflags --libs x11 xinerama x11 xft)

normal:
	$(CC) -o goomwwm goomwwm.c $(CFLAGS) $(LDADD) $(LDFLAGS)

debug:
	$(CC) -o goomwwm-debug goomwwm.c $(CFLAGS) -g -DDEBUG $(LDADD)

proto:
	cat *.c | egrep '^(void|int|char|unsigned|client|Window|winlist|XWindow)' | sed -r 's/\)/);/' > proto.h

all: proto normal debug

clean:
	rm -f goomwwm goomwwm-debug