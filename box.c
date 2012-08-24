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

// Xft text box, optionally editable
box* box_create(Window parent, bitmap flags, short x, short y, short w, short h, char *color)
{
	box *b = allocate_clear(sizeof(box));

	b->flags = flags;
	b->parent = parent;

	b->window = XCreateSimpleWindow(display, b->parent, 0, 0, 1, 1, 0, None, None);

	box_moveresize(b, x, y, w, h);
	box_color(b, color);

	return b;
}

void box_color(box *b, char *color)
{
	b->color = color_get(color);
}

void box_moveresize(box *b, short x, short y, short w, short h)
{
	b->x = x; b->y = y; b->w = MAX(1, w); b->h = MAX(1, h);
	XMoveResizeWindow(display, b->window, b->x, b->y, b->w, b->h);
}

void box_show(box *b)
{
	XMapWindow(display, b->window);
}

void box_hide(box *b)
{
	XUnmapWindow(display, b->window);
}

void box_draw(box *b)
{
	XSetWindowAttributes attr; attr.background_pixel = b->color;
	XChangeWindowAttributes(display, b->window, CWBackPixel, &attr);
	XClearWindow(display, b->window);
}

void box_free(box *b)
{
	XDestroyWindow(display, b->window);
	free(b);
}
