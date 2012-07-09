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


// find the dimensions of the monitor displaying point x,y
void monitor_dimensions(Screen *screen, int x, int y, workarea *mon)
{
	memset(mon, 0, sizeof(workarea));
	mon->w = WidthOfScreen(screen);
	mon->h = HeightOfScreen(screen);

	// locate the current monitor
	if (XineramaIsActive(display))
	{
		int monitors, i;
		XineramaScreenInfo *info = XineramaQueryScreens(display, &monitors);
		if (info)
		{
			for (i = 0; i < monitors; i++)
			{
				if (INTERSECT(x, y, 1, 1, info[i].x_org, info[i].y_org, info[i].width, info[i].height))
				{
					mon->x = info[i].x_org; mon->y = info[i].y_org;
					mon->w = info[i].width; mon->h = info[i].height;
					break;
				}
			}
			XFree(info);
		}
	}
}

// find the dimensions, EXCLUDING STRUTS, of the monitor displaying point x,y
void monitor_dimensions_struts(Screen *screen, int x, int y, workarea *mon)
{
	monitor_dimensions(screen, x, y, mon);
	Window root = RootWindow(display, XScreenNumberOfScreen(screen));

	// strut cardinals are relative to the root window size, which is not necessarily the monitor size
	XWindowAttributes *rattr = window_get_attributes(root);
	int left = 0, right = 0, top = 0, bottom = 0;

	int i; Window win;
	// walk the open apps and check for struts
	// this is fairly lightweight thanks to some caches
	winlist_ascend(windows_in_play(root), i, win)
	{
		XWindowAttributes *attr = window_get_attributes(win);
		if (attr && !attr->override_redirect && attr->root == root
			&& INTERSECT(attr->x, attr->y, attr->width, attr->height, mon->x, mon->y, mon->w, mon->h))
		{
			unsigned long strut[12];
			if (window_get_cardinal_prop(win, netatoms[_NET_WM_STRUT_PARTIAL], strut, 12)
				|| window_get_cardinal_prop(win, netatoms[_NET_WM_STRUT], strut, 4))
			{
				// we only pay attention to the first four params
				// this is no more complex that _NET_WM_STRUT, but newer stuff uses _PARTIAL
				left  = MAX(left, strut[0]); right  = MAX(right,  strut[1]);
				top   = MAX(top,  strut[2]); bottom = MAX(bottom, strut[3]);
			}
		}
	}
	mon->l = MAX(0, left-mon->x);
	mon->r = MAX(0, (mon->x+mon->w)-(rattr->width-right));
	mon->t = MAX(0, top-mon->y);
	mon->b = MAX(0, (mon->y+mon->h)-(rattr->height-bottom));
	mon->x += mon->l; mon->y += mon->t;
	mon->w -= (mon->l+mon->r);
	mon->h -= (mon->t+mon->b);
}

// determine which monitor holds the active window, or failing that the mouse pointer
void monitor_active(Screen *screen, workarea *mon)
{
	Window root = RootWindow(display, XScreenNumberOfScreen(screen));
	client *c = client_active(root, 0);
	if (c && c->focus)
	{
		client_extended_data(c);
		memmove(mon, &c->monitor, sizeof(workarea));
		return;
	}
	int x, y;
	if (pointer_get(root, &x, &y))
	{
		monitor_dimensions_struts(screen, x, y, mon);
		return;
	}
	monitor_dimensions_struts(screen, 0, 0, mon);
}
