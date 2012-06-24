CFLAGS?=-Wall -O2
LDADD?=-I/usr/include/freetype2 -lX11 -lXinerama -lXft

PREFIX?=/usr
BINDIR?=$(PREFIX)/bin

all: normal

normal:
	$(CC) $(CFLAGS) $(LDADD) -o goomwwm goomwwm.c

debug:
	$(CC) $(CFLAGS) -g -DDEBUG $(LDADD) -o goomwwm-debug goomwwm.c

install: all
	install -Dm 755 goomwwm $(DESTDIR)$(BINDIR)/goomwwm

clean:
	rm -f goomwwm goomwwm-debug