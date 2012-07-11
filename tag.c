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

// for the benefit of EWMH type pagers, tag = desktop
// but, since a window can have multiple tags... oh well
unsigned int tag_to_desktop(unsigned int tag)
{
	unsigned int i; for (i = 0; i < TAGS; i++) if (tag & (1<<i)) return i;
	return 0xffffffff;
}

unsigned int desktop_to_tag(unsigned int desktop)
{
	return (desktop == 0xffffffff) ? 0: 1<<desktop;
}

// update current desktop on all roots
void tag_set_current(unsigned int tag)
{
	current_tag = tag; unsigned long d = tag_to_desktop(current_tag);
	int scr; for (scr = 0; scr < ScreenCount(display); scr++)
		window_set_cardinal_prop(RootWindow(display, scr), netatoms[_NET_CURRENT_DESKTOP], &d, 1);
}

// raise all windows in a tag
void tag_raise(unsigned int tag)
{
	int i; Window w; client *c;
	char msg[32]; sprintf(msg, "Tag %d", tag_to_desktop(tag)+1);
	winlist *stack;

	int scr; for (scr = 0; scr < ScreenCount(display); scr++)
	{
		Window root = RootWindow(display, scr);
		XWindowAttributes *attr = window_get_attributes(root);
		winlist *inplay = windows_in_play(root);
		stack = winlist_new();

		// locate windows with _NET_WM_STATE_ABOVE and _NET_WM_STATE_STICKY
		clients_descend(inplay, i, w, c)
			if (winlist_find(stack, w) < 0 && c->visible && c->trans == None
				&& client_has_state(c, netatoms[_NET_WM_STATE_ABOVE])
				&& client_has_state(c, netatoms[_NET_WM_STATE_STICKY]))
					client_stack_family(c, stack);
		// locate windows with _NET_WM_STATE_ABOVE in this tag
		clients_descend(inplay, i, w, c)
			if (winlist_find(stack, w) < 0 && c->visible && c->trans == None
				&& client_has_state(c, netatoms[_NET_WM_STATE_ABOVE]) && c->cache->tags & tag)
					client_stack_family(c, stack);
		// locate _NET_WM_WINDOW_TYPE_DOCK windows
		clients_descend(inplay, i, w, c)
			if (winlist_find(stack, w) < 0 && c->visible && c->trans == None
				&& c->type == netatoms[_NET_WM_WINDOW_TYPE_DOCK])
					client_stack_family(c, stack);
		// locate all other windows in the tag
		managed_descend(root, i, w, c)
			if (winlist_find(stack, w) < 0 && c->trans == None && c->cache->tags & tag)
				client_stack_family(c, stack);
		if (stack->len)
		{
			// raise the top window in the stack
			XRaiseWindow(display, stack->array[0]);
			// stack everything else, in order, underneath top window
			if (stack->len > 1) XRestackWindows(display, stack->array, stack->len);
		}
		winlist_free(stack);
		say(attr->screen, msg);
	}
	// runs on all screens/roots
	tag_set_current(tag);

	// focus the last-focused client in the tag
	clients_descend(windows_activated, i, w, c) if (c->cache->tags & tag)
		{ client_activate(c, RAISE, WARPDEF); break; }
}

// check active client. if
void tag_auto_switch(Window root)
{
	client *c = client_active(root, 0);
	if (c && c->cache->tags && !(c->cache->tags & current_tag))
	{
		int i, n = 0; Window w; client *o; tag_descend(root, i, w, o, current_tag) n++;
		if (!n) tag_raise(desktop_to_tag(tag_to_desktop(c->cache->tags)));
	}
}
