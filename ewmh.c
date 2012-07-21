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

// update _NET_CLIENT_LIST
void ewmh_client_list()
{
	XSync(display, False);
	// this often happens after we've made changes. refresh
	reset_cache_inplay();

	winlist *relevant = winlist_new();
	winlist *mapped   = winlist_new();
	int i; Window w; client *c;

	// windows_in_play() returns the stacking order. windows_activated *MAY NOT* have the same order
	managed_ascend(i, w, c) if (!client_has_state(c, netatoms[_NET_WM_STATE_SKIP_TASKBAR])) winlist_append(relevant, w, NULL);
	XChangeProperty(display, root, netatoms[_NET_CLIENT_LIST_STACKING], XA_WINDOW, 32, PropModeReplace, (unsigned char*)relevant->array, relevant->len);

	// 'windows' list has mapping order of everything. build 'mapped' from 'relevant', ordered by 'windows'
	winlist_ascend(windows, i, w) if (winlist_forget(relevant, w)) winlist_append(mapped, w, NULL);
	XChangeProperty(display, root, netatoms[_NET_CLIENT_LIST], XA_WINDOW, 32, PropModeReplace, (unsigned char*)mapped->array, mapped->len);

	winlist_free(mapped);
	winlist_free(relevant);
}

// update _NET_ACTIVE_WINDOW
void ewmh_active_window(Window w)
{
	XChangeProperty(display, root, netatoms[_NET_ACTIVE_WINDOW], XA_WINDOW, 32, PropModeReplace, (unsigned char*)&w, 1);
}

// _NET_DESKTOP stuff, taking _NET_WM_STRUT* into account
void ewmh_desktop_list()
{
	int i; XWindowAttributes *attr = window_get_attributes(root);
	// nine desktops. want more space? buy more monitors and use xinerama :)
	unsigned long desktops = TAGS, area[4*TAGS], geo[2], view[2], desktop;

	// this will return the full X screen, not Xinerama screen
	workarea mon; monitor_dimensions_struts(-1, -1, &mon);

	// figure out the workarea, less struts
	for (i = 0; i < TAGS; i++)
	{
		area[(i*4)+0] = mon.x; area[(i*4)+1] = mon.y;
		area[(i*4)+2] = mon.w; area[(i*4)+3] = mon.h;
	}
	view[0] = 0; view[1] = 0;
	geo[0] = attr->width; //DisplayWidth(display, XScreenNumberOfScreen(attr->screen));
	geo[1] = attr->height; //DisplayHeight(display, XScreenNumberOfScreen(attr->screen));
	desktop = tag_to_desktop(current_tag);

	window_set_cardinal_prop(root, netatoms[_NET_NUMBER_OF_DESKTOPS], &desktops, 1);
	window_set_cardinal_prop(root, netatoms[_NET_DESKTOP_GEOMETRY],   geo,  2);
	window_set_cardinal_prop(root, netatoms[_NET_DESKTOP_VIEWPORT],   view, 2);
	window_set_cardinal_prop(root, netatoms[_NET_WORKAREA],           area, TAGS*4);
	window_set_cardinal_prop(root, netatoms[_NET_CURRENT_DESKTOP],    &desktop, 1);
}
