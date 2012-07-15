/* GoomwWM, Get out of my way, Window Manager!

MIT/X11 License
Copyright (c) 2012 Sean Pringle <sean.pringle@gmail.com>

Permission is hereby granted, free of charge, to any person obtaining
a copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice shall be
included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

*/

#define _GNU_SOURCE
#include "goomwwm.h"
#include "proto.h"
#include "util.c"
#include "winlist.c"
#include "rule.c"
#include "window.c"
#include "monitor.c"
#include "client.c"
#include "ewmh.c"
#include "tag.c"
#include "menu.c"
#include "handle.c"
#include "grab.c"
#include "cli.c"
#include "wm.c"

int main(int argc, char *argv[])
{
	int i;

	// catch help request
	if (find_arg(argc, argv, "-help") >= 0
		|| find_arg(argc, argv, "--help") >= 0
		|| find_arg(argc, argv, "-h") >= 0)
	{
		fprintf(stderr, "See the man page or visit http://github.com/seanpringle/goomwwm\n");
		return EXIT_FAILURE;
	}

	if(!(display = XOpenDisplay(0)))
	{
		fprintf(stderr, "cannot open display!\n");
		return EXIT_FAILURE;
	}
	signal(SIGCHLD, catch_exit);
	screen = DefaultScreenOfDisplay(display);
	screen_id = DefaultScreen(display);
	root = DefaultRootWindow(display);

	// X atom values
	for (i = 0; i < ATOMS; i++) atoms[i] = XInternAtom(display, atom_names[i], False);
	for (i = 0; i < GATOMS; i++) gatoms[i] = XInternAtom(display, gatom_names[i], False);
	for (i = 0; i < NETATOMS; i++) netatoms[i] = XInternAtom(display, netatom_names[i], False);

	return find_arg(argc, argv, "-cli") >= 0 ? cli_main(argc, argv): wm_main(argc, argv);
}
