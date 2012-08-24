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

// events we're interested in
void window_select(Window w)
{
	XSelectInput(display, w, EnterWindowMask | LeaveWindowMask | FocusChangeMask | PropertyChangeMask);
}

// XGetWindowAttributes with caching
XWindowAttributes* window_get_attributes(Window w)
{
	int idx = winlist_find(cache_xattr, w);
	if (idx < 0)
	{
		XWindowAttributes *cattr = allocate(sizeof(XWindowAttributes));
		if (XGetWindowAttributes(display, w, cattr))
		{
			winlist_append(cache_xattr, w, cattr);
			return cattr;
		}
		free(cattr);
		return NULL;
	}
	return cache_xattr->data[idx];
}

// retrieve a property of any type from a window
int window_get_prop(Window w, Atom prop, Atom *type, int *items, void *buffer, int bytes)
{
	Atom _type; if (!type) type = &_type;
	int _items; if (!items) items = &_items;
	int format; unsigned long nitems, nbytes; unsigned char *ret = NULL;
	memset(buffer, 0, bytes);

	if (XGetWindowProperty(display, w, prop, 0, bytes/4, False, AnyPropertyType, type,
		&format, &nitems, &nbytes, &ret) == Success && ret && *type != None && format)
	{
		if (format ==  8) memmove(buffer, ret, MIN(bytes, nitems));
		if (format == 16) memmove(buffer, ret, MIN(bytes, nitems * sizeof(short)));
		if (format == 32) memmove(buffer, ret, MIN(bytes, nitems * sizeof(long)));
		*items = (int)nitems; XFree(ret);
		return 1;
	}
	return 0;
}

// retrieve a text property from a window
// technically we could use window_get_prop(), but this is better for character set support
char* window_get_text_prop(Window w, Atom atom)
{
	XTextProperty prop; char *res = NULL;
	char **list = NULL; int count;
	if (XGetTextProperty(display, w, &prop, atom) && prop.value && prop.nitems)
	{
		if (prop.encoding == XA_STRING)
		{
			res = allocate(strlen((char*)prop.value)+1);
			strcpy(res, (char*)prop.value);
		}
		else
		if (XmbTextPropertyToTextList(display, &prop, &list, &count) >= Success && count > 0 && *list)
		{
			res = allocate(strlen(*list)+1);
			strcpy(res, *list);
			XFreeStringList(list);
		}
	}
	if (prop.value) XFree(prop.value);
	return res;
}

int window_set_text_prop(Window w, Atom atom, char *txt)
{
	XTextProperty prop;
	if (XStringListToTextProperty(&txt, 1, &prop))
	{
		XSetTextProperty(display, w, &prop, atom);
		XFree(prop.value);
	}
	return 0;
}

int window_get_atom_prop(Window w, Atom atom, Atom *list, int count)
{
	Atom type; int items;
	return window_get_prop(w, atom, &type, &items, list, count*sizeof(Atom)) && type == XA_ATOM ? items:0;
}

void window_set_atom_prop(Window w, Atom prop, Atom *atoms, int count)
{
	XChangeProperty(display, w, prop, XA_ATOM, 32, PropModeReplace, (unsigned char*)atoms, count);
}

int window_get_cardinal_prop(Window w, Atom atom, unsigned long *list, int count)
{
	Atom type; int items;
	return window_get_prop(w, atom, &type, &items, list, count*sizeof(unsigned long)) && type == XA_CARDINAL ? items:0;
}

void window_set_cardinal_prop(Window w, Atom prop, unsigned long *values, int count)
{
	XChangeProperty(display, w, prop, XA_CARDINAL, 32, PropModeReplace, (unsigned char*)values, count);
}

void window_unset_prop(Window w, Atom prop)
{
	XDeleteProperty(display, w, prop);
}

// a ClientMessage
// some things, like the built-in window switcher, use an EWMH ClientMessage
// also, older WM_PROTOCOLS type stuff calls this
int window_send_message(Window target, Window subject, Atom atom, unsigned long protocol, unsigned long mask)
{
	XEvent e; memset(&e, 0, sizeof(XEvent));
	e.xclient.type = ClientMessage;
	e.xclient.message_type = atom;     e.xclient.window    = subject;
	e.xclient.data.l[0]    = protocol; e.xclient.data.l[1] = latest;
	e.xclient.send_event   = True;     e.xclient.format    = 32;
	int r = XSendEvent(display, target, False, mask, &e) ?1:0;
	XFlush(display);
	return r;
}

// top-level, visible windows. DOES include non-managable docks/panels
winlist* windows_in_play()
{
	if (cache_inplay->len) return cache_inplay;

	unsigned int nwins; int i; Window w1, w2, *wins;
	if (XQueryTree(display, root, &w1, &w2, &wins, &nwins) && wins)
	{
		for (i = 0; i < nwins; i++)
		{
			XWindowAttributes *attr = window_get_attributes(wins[i]);
			if (attr && attr->override_redirect == False && attr->map_state == IsViewable)
				winlist_append(cache_inplay, wins[i], NULL);
		}
	}
	if (wins) XFree(wins);
	return cache_inplay;
}

// top-level windows, visible or not. DOES include non-managable docks/panels
winlist* window_children()
{
	winlist *l = winlist_new();
	unsigned int nwins; int i; Window w1, w2, *wins;
	if (XQueryTree(display, root, &w1, &w2, &wins, &nwins) && wins)
	{
		for (i = 0; i < nwins; i++)
		{
			XWindowAttributes *attr = window_get_attributes(wins[i]);
			if (attr && attr->override_redirect == False && (attr->map_state == IsUnmapped || attr->map_state == IsViewable))
				winlist_append(l, wins[i], NULL);
		}
	}
	if (wins) XFree(wins);
	return l;
}

// the window on top of windows_activated list was the last one we activated
// assume this is still the active one... seems to work most of the time!
// if this is wrong, worst case scenario is focus manages to revert to root
int window_is_active(Window w)
{
	return windows_activated->len && w == windows_activated->array[windows_activated->len-1] ?1:0;
}
