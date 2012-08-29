CFLAGS?=-Wall -O2
LDADD?=$(shell pkg-config --cflags --libs x11 xinerama x11 xft)

normal:
	$(CC) -o goomwwm goomwwm.c $(CFLAGS) $(LDADD) $(LDFLAGS)

debug:
	$(CC) -o goomwwm-debug goomwwm.c $(CFLAGS) -g -DDEBUG $(LDADD)

proto:
	cat *.c | egrep '^(void|int|char|unsigned|client|Window|winlist|box|textbox|XWindow)' | sed -r 's/\)/);/' > proto.h

docs:
	pandoc -s -w man goomwwm.md -o goomwwm.1

all: proto normal debug docs

clean:
	rm -f goomwwm goomwwm-debug