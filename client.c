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

// manipulate client _NET_WM_STATE_*

void client_flush_state(client *c)
{
	window_set_atom_prop(c->window, netatoms[_NET_WM_STATE], c->state, c->states);
}

int client_has_state(client *c, Atom state)
{
	int i; for (i = 0; i < c->states; i++) if (c->state[i] == state) return 1;
	return 0;
}

void client_add_state(client *c, Atom state)
{
	if (c->states < CLIENTSTATE && !client_has_state(c, state))
	{
		c->state[c->states++] = state;
		client_flush_state(c);
	}
}

void client_remove_state(client *c, Atom state)
{
	if (!client_has_state(c, state)) return;
	Atom newstate[CLIENTSTATE]; int i, n;
	for (i = 0, n = 0; i < c->states; i++) if (c->state[i] != state) newstate[n++] = c->state[i];
	memmove(c->state, newstate, sizeof(Atom)*n); c->states = n;
	client_flush_state(c);
}

void client_remove_all_states(client *c)
{
	memset(&c->state, 0, sizeof(Atom)*CLIENTSTATE);
	c->states = 0; client_flush_state(c);
}

void client_set_state(client *c, Atom state, int on)
{
	if (on) client_add_state(c, state); else client_remove_state(c, state);
}

// extend client data
void client_descriptive_data(client *c)
{
	if (!c || c->is_described) return;

	char *name;
	if ((name = window_get_text_prop(c->window, netatoms[_NET_WM_NAME])) && name)
		c->title = name;
	else
	if (XFetchName(display, c->window, &name))
	{
		c->title = strdup(name);
		XFree(name);
	}
	XClassHint chint;
	if (XGetClassHint(display, c->window, &chint))
	{
		c->class = strdup(chint.res_class);
		c->name  = strdup(chint.res_name);
		XFree(chint.res_class); XFree(chint.res_name);
	}
	c->is_described = 1;
}

// extend client data
// necessary for anything that is going to move/resize/stack, but expensive to do
// every time in client_create()
void client_extended_data(client *c)
{
	if (!c || c->is_extended) return;

	long sr; XGetWMNormalHints(display, c->window, &c->xsize, &sr);
	monitor_dimensions_struts(c->x+c->w/2, c->y+c->h/2, &c->monitor);

	int screen_x = c->monitor.x, screen_y = c->monitor.y;
	int screen_width = c->monitor.w, screen_height = c->monitor.h;
	int vague = MAX(screen_width/100, screen_height/100);

	// window co-ords translated to 0-based on screen
	// co-ords are x,y upper left outsize border, w,h inside border
	int x = c->xattr.x - screen_x - c->border_width;
	int y = c->xattr.y - screen_y - c->border_width - c->titlebar_height;
	int w = c->xattr.width  + c->border_width*2;
	int h = c->xattr.height + c->border_width*2 + c->titlebar_height;

	c->x = screen_x + x;
	c->y = screen_y + y;
	c->w = w; c->h = h;

	// gather info on the current window position, so we can try and resize and move nicely
	c->is_full    = (x < 1 && y < 1 && w >= screen_width && h >= screen_height) ? 1:0;
	c->is_left    = c->is_full || NEAR(0, vague, x);
	c->is_top     = c->is_full || NEAR(0, vague, y);
	c->is_right   = c->is_full || NEAR(screen_width, vague, x+w);
	c->is_bottom  = c->is_full || NEAR(screen_height, vague, y+h);
	c->is_xcenter = c->is_full || NEAR((screen_width-w)/2,  vague, x) ? 1:0;
	c->is_ycenter = c->is_full || NEAR((screen_height-h)/2, vague, y) ? 1:0;
	c->is_maxh    = c->is_full || (c->is_left && w >= screen_width-2);
	c->is_maxv    = c->is_full || (c->is_top && h >= screen_height-2);

	c->is_extended = 1;
}

// true if a client window matches a rule pattern
int client_rule_match(client *c, winrule *r)
{
	if (c->trans
		|| c->type == netatoms[_NET_WM_WINDOW_TYPE_DESKTOP]
		|| c->type == netatoms[_NET_WM_WINDOW_TYPE_DOCK]
		// EWMH seems to often equate dialogs with transient_for, and we already ignore transients...
		|| c->type == netatoms[_NET_WM_WINDOW_TYPE_DIALOG]
		// following should never be children of the root window anyway? hence never managed or ruled. ignore anyway...
		|| c->type == netatoms[_NET_WM_WINDOW_TYPE_DROPDOWN_MENU]
		|| c->type == netatoms[_NET_WM_WINDOW_TYPE_POPUP_MENU]
		|| c->type == netatoms[_NET_WM_WINDOW_TYPE_TOOLTIP]
		|| c->type == netatoms[_NET_WM_WINDOW_TYPE_NOTIFICATION]
		|| c->type == netatoms[_NET_WM_WINDOW_TYPE_COMBO]
		) return 0;
		// _NET_WM_WINDOW_TYPE_SPLASH can be annoying, so we do let them be ruled
		// _NET_WM_WINDOW_TYPE_UTILITY and TOOLBAR are both persistent and may be managed and ruled
	client_descriptive_data(c);
	if (strchr(r->pattern, ':') && strchr("cnte", r->pattern[0]))
	{
		if (r->pattern[0] == 'c') return regexec(&r->re, c->class, 0, NULL, 0) ?0:1;
		if (r->pattern[0] == 'n') return regexec(&r->re, c->name,  0, NULL, 0) ?0:1;
		if (r->pattern[0] == 't') return regexec(&r->re, c->title, 0, NULL, 0) ?0:1;
		// check if window on edge:(top|left|bottom|right)
		if (r->pattern[0] == 'e')
		{
			client_extended_data(c);
			char *p = strchr(r->pattern, ':')+1;
			if (!strcasecmp(p, "top")    && c->is_top)    return 1;
			if (!strcasecmp(p, "bottom") && c->is_bottom) return 1;
			if (!strcasecmp(p, "left")   && c->is_left)   return 1;
			if (!strcasecmp(p, "right")  && c->is_right)  return 1;
		}
		return 0;
	}
	return (
		regexec(&r->re, c->class, 0, NULL, 0) == 0 ||
		regexec(&r->re, c->name,  0, NULL, 0) == 0 ||
		regexec(&r->re, c->title, 0, NULL, 0) == 0) ?1:0;
}

// find a client's rule, optionally filtered by flags
winrule* client_rule(client *c, bitmap flags)
{
	if (!c->is_ruled)
	{
		c->rule = config_rules; while (c->rule && !client_rule_match(c, c->rule)) c->rule = c->rule->next;
		c->is_ruled = 1;
	}
	return (!c->rule || (flags && !(flags & c->rule->flags))) ? NULL: c->rule;
}

// collect info on any window
// doesn't have to be a window we'll end up managing
client* client_create(Window win)
{
	if (win == None) return NULL;
	int idx = winlist_find(cache_client, win);
	if (idx >= 0) return cache_client->data[idx];

	// if this fails, we're up that creek
	XWindowAttributes *attr = window_get_attributes(win);
	if (!attr) return NULL;

	client *c = allocate_clear(sizeof(client));
	c->window = win; c->title = c->name = c->class = empty;
	// copy xattr so we don't have to care when stuff is freed
	memmove(&c->xattr, attr, sizeof(XWindowAttributes));
	XGetTransientForHint(display, win, &c->trans);

	// find last known state
	wincache *cache = NULL;
	idx = winlist_find(windows, c->window);
	if (idx < 0)
	{
		cache = allocate_clear(sizeof(wincache));
		winlist_append(windows, c->window, cache);
		idx = windows->len-1;
	}
	// the cache is not tightly linked to the window at all
	// if it's populated, it gets used to make behaviour appear logically
	// if it's empty, nothing cares that much, or it gets initialized
	c->cache = windows->data[idx];

	c->visible = c->xattr.map_state == IsViewable ?1:0;
	c->states  = window_get_atom_prop(win, netatoms[_NET_WM_STATE], c->state, CLIENTSTATE);
	window_get_atom_prop(win, netatoms[_NET_WM_WINDOW_TYPE], &c->type, 1);

	if (c->type == None) c->type = (c->trans != None)
		// trasients default to dialog
		? netatoms[_NET_WM_WINDOW_TYPE_DIALOG]
		// non-transients default to normal
		: netatoms[_NET_WM_WINDOW_TYPE_NORMAL];

	c->manage = c->xattr.override_redirect == False && !c->cache->is_ours
		&& c->type != netatoms[_NET_WM_WINDOW_TYPE_DESKTOP]
		&& c->type != netatoms[_NET_WM_WINDOW_TYPE_NOTIFICATION]
		&& c->type != netatoms[_NET_WM_WINDOW_TYPE_DOCK]
		&& c->type != netatoms[_NET_WM_WINDOW_TYPE_SPLASH]
		?1:0;

	c->active    = c->manage && c->visible && window_is_active(c->window) ?1:0;
	c->minimized = winlist_find(windows_minimized, c->window) >= 0 ? 1:0;
	c->shaded    = winlist_find(windows_shaded, c->window) >= 0 ? 1:0;
	c->urgent    = c->manage && client_has_state(c, netatoms[_NET_WM_STATE_DEMANDS_ATTENTION]) ? 1:0;

	// extra checks for managed windows
	if (c->manage && client_rule(c, RULE_IGNORE)) c->manage = 0;

	// focus seems a really dodgy way to determine the "active" window, but in some
	// cases checking both ->active and ->focus is necessary to bahave logically
	Window focus; int rev;
	XGetInputFocus(display, &focus, &rev);
	c->focus = focus == win ? 1:0;

	XWMHints *hints = XGetWMHints(display, win);
	if (hints)
	{
		c->input = hints->flags & InputHint && hints->input ? 1: 0;
		c->initial_state = hints->flags & StateHint ? hints->initial_state: NormalState;
		c->urgent = c->urgent || hints->flags & XUrgencyHint ? 1: 0;
		XFree(hints);
	}

	c->decorate = c->manage;
	// can't get away with ignoring old motif stuff, as some apps use it
	Atom motif_type; int motif_items; motif_hints mhints;
	if (window_get_prop(c->window, atoms[_MOTIF_WM_HINTS], &motif_type, &motif_items, &mhints, sizeof(mhints)) && motif_items)
		if (mhints.flags & 2 && mhints.decorations == 0) c->decorate = 0;

	// co-ords include borders
	c->x = c->xattr.x; c->y = c->xattr.y; c->w = c->xattr.width; c->h = c->xattr.height;
	c->border_width = c->decorate && !client_has_state(c, netatoms[_NET_WM_STATE_FULLSCREEN]) ? config_border_width: 0;
	c->titlebar_height = c->decorate && !client_has_state(c, netatoms[_NET_WM_STATE_FULLSCREEN]) ? config_titlebar_height: 0;
	// compenstate for borders on non-fullscreen windows
	if (c->decorate)
	{
		c->x -= c->border_width;
		c->y -= c->border_width + c->titlebar_height;
		c->w += c->border_width*2;
		c->h += c->border_width*2 + c->titlebar_height;
	}
	// check whether the frame should be created
	if (c->decorate && !c->cache->frame)
	{
		c->cache->frame = box_create(root, 0, c->x, c->y, c->w, c->h, config_border_blur);

		cache = allocate_clear(sizeof(wincache));
		winlist_append(windows, c->cache->frame->window, cache);
		cache->is_ours = 1;
		// associate with the window (see handle_buttonpress)
		cache->app = c->window;

		XSelectInput(display, c->cache->frame->window, ExposureMask);

		// stack frame under client window
		Window wins[2] = { c->window, c->cache->frame->window };
		XRestackWindows(display, wins, 2);

		// ...and same for titlebar
		if (config_titlebar_height)
		{
			client_extended_data(c);
			c->cache->title = textbox_create(c->cache->frame->window,
				TB_CENTER, 0, c->border_width, c->w, config_titlebar_height,
				config_titlebar_font, config_titlebar_focus, config_border_focus,
				c->title, NULL);
			XSelectInput(display, c->cache->title->window, ExposureMask);
			textbox_show(c->cache->title);
		}
	}

	winlist_append(cache_client, c->window, c);
	return c;
}

// refresh client_cache
client* client_recreate(Window w)
{
	int idx = winlist_find(cache_client, w);
	if (idx >= 0)
	{
		client_free(cache_client->data[idx]);
		cache_client->data[idx] = NULL;
		winlist_forget(cache_client, w);
	}
	return client_create(w);
}

// release client memory. this should only be called during the global cache resets
void client_free(client *c)
{
	if (!c) return;
	if (c->title != empty) free(c->title);
	if (c->class != empty) free(c->class);
	if (c->name  != empty) free(c->name);
	free(c);
}

// true if client windows overlap
int clients_intersect(client *a, client *b)
{
	client_extended_data(a); client_extended_data(b);
	return INTERSECT(a->x, a->y, a->w, a->h, b->x, b->y, b->w, b->h) ?1:0;
}

// if a client supports a WM_PROTOCOLS type atom, dispatch an event
int client_protocol_event(client *c, Atom protocol)
{
	Atom *protocols = NULL;
	int i, found = 0, num_pro = 0;
	if (XGetWMProtocols(display, c->window, &protocols, &num_pro))
		for (i = 0; i < num_pro && !found; i++)
			if (protocols[i] == protocol) found = 1;
	if (found)
		window_send_message(c->window, c->window, atoms[WM_PROTOCOLS], protocol, NoEventMask);
	if (protocols) XFree(protocols);
	return found;
}

// close a window politely if possible, else kill it
void client_close(client *c)
{
	// prevent frame flash
	c->active = 0;
	client_redecorate(c);
	if (c->cache->frame) box_hide(c->cache->frame);

	if (c->cache->have_closed || !client_protocol_event(c, atoms[WM_DELETE_WINDOW]))
		XKillClient(display, c->window);

	c->cache->have_closed = 1;
}

// true if x/y is over a visible portion of the client window
int client_warp_check(client *c, int x, int y)
{
	int i, ok = 1; Window w; client *o;
	managed_descend(i, w, o)
	{
		if (!ok || w == c->window) break;
		if (INTERSECT(o->x, o->y, o->w, o->h, x, y, 1, 1)) ok = 0;
	}
	return ok;
}

// ensure the pointer is over a specific client
void client_warp_pointer(client *c)
{
	// needs the updated stacking mode, so clear cache
	XSync(display, False);
	reset_cache_inplay();

	client_extended_data(c);
	int vague = MAX(c->monitor.w/100, c->monitor.h/100);
	int x, y; if (!pointer_get(&x, &y)) return;
	int mx = x, my = y;
	// if pointer is not already over the client...
	if (!INTERSECT(c->x, c->y, c->w, c->h, x, y, 1, 1) || !client_warp_check(c, x, y))
	{
		int overlap_x = OVERLAP(c->x, c->w, x, 1);
		int overlap_y = OVERLAP(c->y, c->h, y, 1);
		int xd = 0, yd = 0;
		if (overlap_y && x < c->x) { x = c->x; xd = vague; }
		if (overlap_y && x > c->x) { x = MIN(x, c->x+c->w-1); xd = 0-vague; }
		if (overlap_x && y < c->y) { y = c->y; yd = vague; }
		if (overlap_x && y > c->y) { y = MIN(y, c->y+c->h-1); yd = 0-vague; }
		// step toward client window
		while ((xd || yd ) && INTERSECT(c->x, c->y, c->w, c->h, x, y, 1, 1) && !client_warp_check(c, x, y))
			{ x += xd; y += yd; }
		// ensure pointer is slightly inside border
		x = MIN(c->x+c->w-vague, MAX(c->x+vague, x));
		y = MIN(c->y+c->h-vague, MAX(c->y+vague, y));
		XWarpPointer(display, None, None, 0, 0, 0, 0, x-mx, y-my);
	}
}

// adjust co-ordinates to take hints into account, ready for move/resize
void client_process_size_hints(client *c, int *x, int *y, int *w, int *h)
{
	// fw/fh still include borders here
	int fx = *x, fy = *y, fw = *w, fh = *h;
	int dec_w = c->border_width*2, dec_h = c->border_width*2+c->titlebar_height;
	int basew = 0, baseh = 0;

	if (c->xsize.flags & PBaseSize)
	{
		basew = c->xsize.base_width;
		baseh = c->xsize.base_height;
	}
	if (c->xsize.flags & PMinSize)
	{
		// fw/fh still include borders here
		fw = MAX(fw, c->xsize.min_width  + dec_w);
		fh = MAX(fh, c->xsize.min_height + dec_h);
	}
	if (c->xsize.flags & PMaxSize)
	{
		// fw/fh still include borders here
		fw = MIN(fw, c->xsize.max_width  + dec_w);
		fh = MIN(fh, c->xsize.max_height + dec_h);
	}
	if (config_resize_inc && c->xsize.flags & PResizeInc)
	{
		client_descriptive_data(c);
		if (config_resize_inc == RESIZEINC
			|| (config_resize_inc == SMARTRESIZEINC && !regquick(SMARTRESIZEINC_IGNORE, c->class)))
		{
			// fw/fh still include borders here
			fw -= basew + dec_w; fh -= baseh + dec_h;
			fw -= fw % c->xsize.width_inc;
			fh -= fh % c->xsize.height_inc;
			fw += basew + dec_w; fh += baseh + dec_h;
		}
	}
	if (c->xsize.flags & PAspect)
	{
		double ratio = (double) fw / fh;
		double minr  = (double) c->xsize.min_aspect.x / c->xsize.min_aspect.y;
		double maxr  = (double) c->xsize.max_aspect.x / c->xsize.max_aspect.y;
			if (ratio < minr) fh = (int)(fw / minr);
		else if (ratio > maxr) fw = (int)(fh * maxr);
	}
	*x = fx; *y = fy; *w = fw; *h = fh;
}

// move & resize a window nicely, respecting hints and EWMH states
void client_moveresize(client *c, unsigned int flags, int fx, int fy, int fw, int fh)
{
	client_extended_data(c);
	int vague = MAX(c->monitor.w/100, c->monitor.h/100);
	int i; Window win; client *o;
	int xsnap = 0, ysnap = 0;

	// this many be different to the client's current c->monitor...
	workarea monitor; monitor_dimensions_struts(MAX(fx, 0), MAX(fy, 0), &monitor);

	// horz/vert size locks
	if (c->cache->vlock) { fy = c->y; fh = c->h; }
	if (c->cache->hlock) { fx = c->x; fw = c->w; }

	// ensure we match fullscreen/maxv/maxh mode. these override above locks!
	if (client_has_state(c, netatoms[_NET_WM_STATE_FULLSCREEN]))
	{
		fx = monitor.x-monitor.l; fy = monitor.y-monitor.t;
		fw = monitor.w+monitor.l+monitor.r; fh = monitor.h+monitor.t+monitor.b;
	}
	else
	{
		if (client_has_state(c, netatoms[_NET_WM_STATE_MAXIMIZED_HORZ]))
			{ fx = monitor.x; fw = monitor.w; }
		if (client_has_state(c, netatoms[_NET_WM_STATE_MAXIMIZED_VERT]))
			{ fy = monitor.y; fh = monitor.h; }

		// shrink onto screen
		if (!(flags & MR_UNCONSTRAIN))
		{
			fw = MAX(MINWINDOW, MIN(fw, monitor.w));
			fh = MAX(MINWINDOW, MIN(fh, monitor.h));
		}

		client_process_size_hints(c, &fx, &fy, &fw, &fh);

		// bump onto screen
		if (!(flags & MR_UNCONSTRAIN))
		{
			fx = MAX(MIN(fx, monitor.x + monitor.w - fw), monitor.x);
			fy = MAX(MIN(fy, monitor.y + monitor.h - fh), monitor.y);
		}
	}

	// put the window in same general position it was before
	if (flags & MR_SMART)
	{
		// shrinking w. check if we were once in a corner previous-to-last
		// expanding w is already covered by bumping above
		if (c->cache && c->cache->last_corner && c->w > fw)
		{
			if (c->cache->last_corner == TOPLEFT || c->cache->last_corner == BOTTOMLEFT || c->cache->last_corner == CENTERLEFT)
				fx = monitor.x;
			if (c->cache->last_corner == TOPRIGHT || c->cache->last_corner == BOTTOMRIGHT || c->cache->last_corner == CENTERRIGHT)
				fx = monitor.x + monitor.w - fw;
			if (c->cache->last_corner == CENTERTOP || c->cache->last_corner == CENTERBOTTOM)
				fx = monitor.x + (monitor.w - fw)/2;
		}
		// screen center always wins
		else if (c->is_xcenter) fx = monitor.x + ((monitor.w - fw) / 2);
		else if (c->is_left) fx = monitor.x;
		else if (c->is_right) fx = monitor.x + monitor.w - fw;

		// shrinking h. check if we were once in a corner previous-to-last
		// expanding h is already covered by bumping above
		if (c->cache && c->cache->last_corner && c->h > fh)
		{
			if (c->cache->last_corner == TOPLEFT || c->cache->last_corner == TOPRIGHT || c->cache->last_corner == CENTERTOP)
				fy = monitor.y;
			if (c->cache->last_corner == BOTTOMLEFT || c->cache->last_corner == BOTTOMRIGHT || c->cache->last_corner == CENTERBOTTOM)
				fy = monitor.y + monitor.h - fh;
			if (c->cache->last_corner == CENTERLEFT || c->cache->last_corner == CENTERRIGHT)
				fy = monitor.y + (monitor.h - fh)/2;
		}
		// screen center always wins
		else if (c->is_ycenter) fy = monitor.y + ((monitor.h - fh) / 2);
		else if (c->is_top) fy = monitor.y;
		else if (c->is_bottom) fy = monitor.y + monitor.h - fh;
	}

	// snap all edges by moving window
	// built for MotionNotify Button1
	if (flags & MR_SNAP)
	{
		// snap to monitor edges
		if (NEAR(c->monitor.x, vague, fx)) { fx = c->monitor.x; xsnap = 1; }
		if (NEAR(c->monitor.y, vague, fy)) { fy = c->monitor.y; ysnap = 1; }
		if (!xsnap && NEAR(c->monitor.x+c->monitor.w, vague, fx+fw)) { fx = c->monitor.x+c->monitor.w-fw; xsnap = 1; }
		if (!ysnap && NEAR(c->monitor.y+c->monitor.h, vague, fy+fh)) { fy = c->monitor.y+c->monitor.h-fh; ysnap = 1; }
		// snap to window edges
		if (!xsnap || !ysnap)
		{
			winlist *visible = clients_partly_visible(&monitor, 0, c->window);
			clients_descend(visible, i, win, o)
			{
				if (!xsnap && NEAR(o->x, vague, fx)) { fx = o->x; xsnap = 1; }
				if (!ysnap && NEAR(o->y, vague, fy)) { fy = o->y; ysnap = 1; }
				if (!xsnap && NEAR(o->x+o->w, vague, fx)) { fx = o->x+o->w; xsnap = 1; }
				if (!ysnap && NEAR(o->y+o->h, vague, fy)) { fy = o->y+o->h; ysnap = 1; }
				if (!xsnap && NEAR(o->x, vague, fx+fw)) { fx = o->x+-fw; xsnap = 1; }
				if (!ysnap && NEAR(o->y, vague, fy+fh)) { fy = o->y+-fh; ysnap = 1; }
				if (!xsnap && NEAR(o->x+o->w, vague, fx+fw)) { fx = o->x+o->w-fw; xsnap = 1; }
				if (!ysnap && NEAR(o->y+o->h, vague, fy+fh)) { fy = o->y+o->h-fh; ysnap = 1; }
				if (xsnap && ysnap) break;
			}
			winlist_free(visible);
		}
	}
	else
	// snap right and bottom edges by resizing window
	// built for MotionNotify Button3
	if (flags & MR_SNAPWH)
	{
		// snap to monitor edges
		if (NEAR(c->monitor.x+c->monitor.w, vague, fx+fw)) { fw = c->monitor.x+c->monitor.w-fx; xsnap = 1; }
		if (NEAR(c->monitor.y+c->monitor.h, vague, fy+fh)) { fh = c->monitor.y+c->monitor.h-fy; ysnap = 1; }
		// snap to window edges
		if (!xsnap || !ysnap)
		{
			winlist *visible = clients_partly_visible(&monitor, 0, c->window);
			clients_descend(visible, i, win, o)
			{
				if (!xsnap && NEAR(o->x, vague, fx+fw)) { fw = o->x-fx; xsnap = 1; }
				if (!ysnap && NEAR(o->y, vague, fy+fh)) { fh = o->y-fy; ysnap = 1; }
				if (!xsnap && NEAR(o->x+o->w, vague, fx+fw)) { fw = o->x+o->w-fx; xsnap = 1; }
				if (!ysnap && NEAR(o->y+o->h, vague, fy+fh)) { fh = o->y+o->h-fy; ysnap = 1; }
				if (xsnap && ysnap) break;
			}
			winlist_free(visible);
		}
	}

	// this needs to occur despite MR_UNCONSTRAIN
	fw = MAX(MINWINDOW, fw);
	fh = MAX(MINWINDOW, fh);

	// update window co-ords for subsequent operations before caches are reset
	c->x = fx; c->y = fy; c->w = fw; c->h = fh;
	memmove(&c->monitor, &monitor, sizeof(workarea));

	// compensate for border on non-fullscreen windows
	if (c->decorate && !client_has_state(c, netatoms[_NET_WM_STATE_FULLSCREEN]))
	{
		fx += c->border_width;
		fy += c->border_width + c->titlebar_height;
		fw = MAX(1, fw - c->border_width*2);
		fh = MAX(1, fh - c->border_width*2 - c->titlebar_height);
	}
	if (c->decorate) box_moveresize(c->cache->frame, c->x, c->y, c->w, c->h);
	XMoveResizeWindow(display, c->window, fx, fy, fw, fh);
	client_redecorate(c);
}

// record a window's size and position in the undo log
void client_commit(client *c)
{
	client_extended_data(c);
	int levels = 0; winundo *undo = c->cache->undo;
	// count current undo chain length for this window
	while (undo) { levels++; undo = undo->next; }

	if (levels > 0)
	{
		// check if the most recent undo state matches current state. if so, no point recording
		undo = c->cache->undo;
		if (undo->x == c->x && undo->y == c->y && undo->w == c->w && undo->h == c->h) return;
	}
	// LIFO up to UNDO cells deep
	if (levels == UNDO)
	{
		undo = c->cache->undo;
		// find second last link
		while (undo->next && undo->next->next) undo = undo->next;
		// chop of the last link
		free(undo->next); undo->next = NULL;
	}
	undo = allocate_clear(sizeof(winundo));
	undo->next = c->cache->undo; c->cache->undo = undo;
	// do the actual snapshot
	undo->x = c->x; undo->y = c->y; undo->w = c->w; undo->h = c->h;
	for (undo->states = 0; undo->states < c->states; undo->states++)
		undo->state[undo->states] = c->state[undo->states];
}

// move/resize a window back to it's last known size and position
void client_rollback(client *c)
{
	if (c->cache->undo)
	{
		// remove most recent winundo from the undo chain
		winundo *undo = c->cache->undo; c->cache->undo = undo->next;
		// do the actual rollback
		for (c->states = 0; c->states < undo->states; c->states++)
			c->state[c->states] = undo->state[c->states];
		client_flush_state(c);
		client_moveresize(c, 0, undo->x, undo->y, undo->w, undo->h);
		free(undo);
	}
}

// save co-ords for later flip-back
// these may MAY BE dulicated in the undo log, but they must remain separate
// to allow proper toggle behaviour for maxv/maxh
void client_save_position(client *c)
{
	client_extended_data(c);
	if (!c->cache) return;

	if (!c->cache->ewmh)
		c->cache->ewmh = allocate_clear(sizeof(winundo));

	winundo *undo = c->cache->ewmh;

	undo->x = c->x; undo->y = c->y;
	undo->w = c->w; undo->h = c->h;
}

// save co-ords for later flip-back
void client_save_position_horz(client *c)
{
	client_extended_data(c); if (!c->cache) return;
	if (!c->cache->ewmh) client_save_position(c);

	winundo *undo = c->cache->ewmh;
	undo->x = c->x; undo->w = c->w;
}

// save co-ords for later flip-back
void client_save_position_vert(client *c)
{
	client_extended_data(c); if (!c->cache) return;
	if (!c->cache->ewmh) client_save_position(c);

	winundo *undo = c->cache->ewmh;
	undo->y = c->y; undo->h = c->h;
}

// revert to saved co-ords
void client_restore_position(client *c, unsigned int smart, int x, int y, int w, int h)
{
	client_extended_data(c);
	client_moveresize(c, smart,
		c->cache && c->cache->ewmh ? c->cache->ewmh->x: x,
		c->cache && c->cache->ewmh ? c->cache->ewmh->y: y,
		c->cache && c->cache->ewmh ? c->cache->ewmh->w: w,
		c->cache && c->cache->ewmh ? c->cache->ewmh->h: h);
}

// revert to saved co-ords
void client_restore_position_horz(client *c, unsigned int smart, int x, int w)
{
	client_extended_data(c);
	client_moveresize(c, smart,
		c->cache && c->cache->ewmh ? c->cache->ewmh->x: x, c->y,
		c->cache && c->cache->ewmh ? c->cache->ewmh->w: w, c->h);
}

// revert to saved co-ords
void client_restore_position_vert(client *c, unsigned int smart, int y, int h)
{
	client_extended_data(c);
	client_moveresize(c, smart,
		c->x,  c->cache && c->cache->ewmh ? c->cache->ewmh->y: y,
		c->w, c->cache && c->cache->ewmh ? c->cache->ewmh->h: h);
}

// build list of unobscured windows within a workarea
winlist* clients_fully_visible(workarea *zone, unsigned int tag, Window ignore)
{
	winlist *hits = winlist_new();
	winlist *inplay = windows_in_play();
	// list of coords/sizes for all windows on this desktop
	workarea *allregions = allocate_clear(sizeof(workarea) * inplay->len);

	int i; Window win; client *o;
	tag_descend(i, win, o, tag)
	{
		client_extended_data(o);
		// only concerned about windows in the zone
		if (ignore != o->window && INTERSECT(o->x, o->y, o->w, o->h, zone->x, zone->y, zone->w, zone->h))
		{
			int j, obscured = 0;
			for (j = inplay->len-1; j > i; j--)
			{
				// if the window intersects with any other window higher in the stack order, it must be at least partially obscured
				if (allregions[j].w && INTERSECT(o->x, o->y, o->w, o->h,
					allregions[j].x, allregions[j].y, allregions[j].w, allregions[j].h))
						{ obscured = 1; break; }
			}
			// record a full visible window
			if (!obscured && o->x >= zone->x && o->y >= zone->y && (o->x + o->w) <= (zone->x + zone->w) && (o->y + o->h) <= (zone->y + zone->h))
				winlist_append(hits, o->window, NULL);
			allregions[i].x = o->x; allregions[i].y = o->y;
			allregions[i].w = o->w; allregions[i].h = o->h;
		}
	}
	// return it in stacking order, bottom to top
	winlist_reverse(hits);
	free(allregions);
	return hits;
}

// build list of unobscured windows within a workarea
winlist* clients_partly_visible(workarea *zone, unsigned int tag, Window ignore)
{
	winlist *hits = winlist_new();
	winlist *inplay = windows_in_play();
	// list of coords/sizes for all windows on this desktop
	workarea *allregions = allocate_clear(sizeof(workarea) * inplay->len);

	int i; Window win; client *o;
	tag_descend(i, win, o, tag)
	{
		client_extended_data(o);
		// only concerned about windows in the zone
		if (ignore != o->window && INTERSECT(o->x, o->y, o->w, o->h, zone->x, zone->y, zone->w, zone->h))
		{
			int j, c1 = 0, c2 = 0, c3 = 0, c4 = 0;
			for (j = inplay->len-1; j > i; j--)
			{
				if (!allregions[j].w) continue;
				// if the window's corners intersects with any other window higher in the stack order, assume it is covered
				if (INTERSECT(o->x, o->y, 1, 1, allregions[j].x, allregions[j].y, allregions[j].w, allregions[j].h)) c1 = 1;
				if (INTERSECT(o->x, o->y+o->h-1, 1, 1, allregions[j].x, allregions[j].y, allregions[j].w, allregions[j].h)) c2 = 1;
				if (INTERSECT(o->x+o->w-1, o->y, 1, 1, allregions[j].x, allregions[j].y, allregions[j].w, allregions[j].h)) c3 = 1;
				if (INTERSECT(o->x+o->w-1, o->y+o->h-1, 1, 1, allregions[j].x, allregions[j].y, allregions[j].w, allregions[j].h)) c4 = 1;
				if (c1 && c2 && c3 && c4) break;
			}
			// record a full visible window
			if ((!c1 || !c2 || !c3 || !c4) && o->x >= zone->x && o->y >= zone->y && (o->x + o->w) <= (zone->x + zone->w) && (o->y + o->h) <= (zone->y + zone->h))
				winlist_append(hits, o->window, NULL);
			allregions[i].x = o->x; allregions[i].y = o->y;
			allregions[i].w = o->w; allregions[i].h = o->h;
		}
	}
	// return it in stacking order, bottom to top
	winlist_reverse(hits);
	free(allregions);
	return hits;
}

// expand a window to take up available space around it on the current monitor
// do not cover any window that is entirely visible (snap to surrounding edges)
void client_expand(client *c, int directions, int x1, int y1, int w1, int h1, int mx, int my, int mw, int mh)
{
	client_extended_data(c);

	// hlock/vlock reduce the area we should look at
	if (c->cache->hlock) { mx = c->x; mw = c->w; if (!mh) { my = c->monitor.y; mh = c->monitor.h; } }
	if (c->cache->vlock) { my = c->y; mh = c->h; if (!mw) { mx = c->monitor.x; mw = c->monitor.w; } }

	// expand only cares about fully visible windows. partially or full obscured windows == free space
	winlist *visible = clients_fully_visible(&c->monitor, c->cache->tags, c->window);

	// list of coords/sizes for fully visible windows on this desktop
	workarea *regions = allocate_clear(sizeof(workarea) * visible->len);

	int i, n = 0, relevant = visible->len; Window win; client *o;
	clients_descend(visible, i, win, o)
	{
		client_extended_data(o);
		if ((mw || mh) && !INTERSECT(o->x, o->y, o->w, o->h, mx, my, mw, mh)) continue;
		regions[n].x = o->x; regions[n].y = o->y;
		regions[n].w = o->w; regions[n].h = o->h;
		n++;
	}

	int x = c->x, y = c->y, w = c->w, h = c->h;
	if (w1 || h1) { x = x1; y = y1; w = w1; h = h1; }

	if (directions & VERTICAL)
	{
		// try to grow upward. locate the lower edge of the nearest fully visible window
		for (n = c->monitor.y, i = 0; i < relevant; i++)
			if (regions[i].y + regions[i].h <= y && OVERLAP(x, w, regions[i].x, regions[i].w))
				n = MAX(n, regions[i].y + regions[i].h);
		h += y-n; y = n;
		// try to grow downward. locate the upper edge of the nearest fully visible window
		for (n = c->monitor.y + c->monitor.h, i = 0; i < relevant; i++)
			if (regions[i].y >= y+h && OVERLAP(x, w, regions[i].x, regions[i].w))
				n = MIN(n, regions[i].y);
		h = n-y;
	}
	if (directions & HORIZONTAL)
	{
		// try to grow left. locate the right edge of the nearest fully visible window
		for (n = c->monitor.x, i = 0; i < relevant; i++)
			if (regions[i].x + regions[i].w <= x && OVERLAP(y, h, regions[i].y, regions[i].h))
				n = MAX(n, regions[i].x + regions[i].w);
		w += x-n; x = n;
		// try to grow right. locate the left edge of the nearest fully visible window
		for (n = c->monitor.x + c->monitor.w, i = 0; i < relevant; i++)
			if (regions[i].x >= x+w && OVERLAP(y, h, regions[i].y, regions[i].h))
				n = MIN(n, regions[i].x);
		w = n-x;
	}
	// optionally limit final size to a bounding box
	if (mw || mh)
	{
		if (x < mx) { w -= mx-x; x = mx; }
		if (y < my) { h -= my-y; y = my; }
		w = MIN(w, mw);
		h = MIN(h, mh);
	}
	client_commit(c);
	client_moveresize(c, 0, x, y, w, h);
	// if we looked like we could expand, but couldn't due to some condition in client_moveresize(),
	// act like a toggle and rollback instead
	winundo *undo = c->cache->undo;
	if (undo->x == c->x && undo->y == c->y && undo->w == c->w && undo->h == c->h)
	{
		// yes, twice!
		client_rollback(c);
		client_rollback(c);
	}
	free(regions);
	winlist_free(visible);
}

// shrink to fit into an empty gap underneath. opposite of client_expand()
void client_contract(client *c, int directions)
{
	client_extended_data(c);
	// cheat and shrink the window absurdly so it becomes just another expansion
	if (directions & VERTICAL && directions & HORIZONTAL)
		client_expand(c, directions, c->x+(c->w/2), c->y+(c->h/2), 5, 5, c->x, c->y, c->w, c->h);
	else
	if (directions & VERTICAL)
		client_expand(c, directions, c->x, c->y+(c->h/2), c->w, 5, c->x, c->y, c->w, c->h);
	else
	if (directions & HORIZONTAL)
		client_expand(c, directions, c->x+(c->w/2), c->y, 5, c->h, c->x, c->y, c->w, c->h);
}

// move or resize a client window to snap to someone else's leading or trailing edge
void client_snapto(client *c, int direction)
{
	client_extended_data(c);

	// hlock/vlock may block this
	if (c->cache->hlock && (direction == SNAPLEFT || direction == SNAPRIGHT)) return;
	if (c->cache->vlock && (direction == SNAPUP   || direction == SNAPDOWN )) return;

	// expand only cares about fully visible windows. partially or full obscured windows == free space
	winlist *visible = clients_partly_visible(&c->monitor, c->cache->tags, c->window);

	// list of coords/sizes for fully visible windows on this desktop
	workarea *regions = allocate_clear(sizeof(workarea) * visible->len);

	int i, n = 0, relevant = visible->len; Window win; client *o;
	clients_descend(visible, i, win, o)
	{
		client_extended_data(o);
		regions[n].x = o->x; regions[n].y = o->y;
		regions[n].w = o->w; regions[n].h = o->h;
		n++;
	}

	int x = c->x, y = c->y, w = c->w, h = c->h;

	if (direction == SNAPUP)
	{
		y--;
		for (n = c->monitor.y, i = 0; i < relevant; i++)
		{
			if (!OVERLAP(c->x-1, c->w+2, regions[i].x, regions[i].w)) continue;
			if (regions[i].y + regions[i].h <= y) n = MAX(n, regions[i].y + regions[i].h);
			if (regions[i].y <= y) n = MAX(n, regions[i].y);
			if (regions[i].y + regions[i].h <= y+h) n = MAX(n, regions[i].y + regions[i].h - h);
			if (regions[i].y <= y+h) n = MAX(n, regions[i].y - h);
		}
		y = n;
	}
	if (direction == SNAPDOWN)
	{
		y++;
		for (n = c->monitor.y + c->monitor.h, i = 0; i < relevant; i++)
		{
			if (!OVERLAP(c->x-1, c->w+2, regions[i].x, regions[i].w)) continue;
			if (regions[i].y + regions[i].h >= y+h) n = MIN(n, regions[i].y + regions[i].h);
			if (regions[i].y >= y+h) n = MIN(n, regions[i].y);
			if (regions[i].y + regions[i].h >= y) n = MIN(n, regions[i].y + regions[i].h + h);
			if (regions[i].y >= y) n = MIN(n, regions[i].y + h);
		}
		y = n-h;
	}
	if (direction == SNAPLEFT)
	{
		x--;
		for (n = c->monitor.x, i = 0; i < relevant; i++)
		{
			if (!OVERLAP(c->y-1, c->h+2, regions[i].y, regions[i].h)) continue;
			if (regions[i].x + regions[i].w <= x) n = MAX(n, regions[i].x + regions[i].w);
			if (regions[i].x <= x) n = MAX(n, regions[i].x);
			if (regions[i].x + regions[i].w <= x+w) n = MAX(n, regions[i].x + regions[i].w - w);
			if (regions[i].x <= x+w) n = MAX(n, regions[i].x - w);
		}
		x = n;
	}
	if (direction == SNAPRIGHT)
	{
		x++;
		for (n = c->monitor.x + c->monitor.w, i = 0; i < relevant; i++)
		{
			if (!OVERLAP(c->y-1, c->h+2, regions[i].y, regions[i].h)) continue;
			if (regions[i].x + regions[i].w >= x+w) n = MIN(n, regions[i].x + regions[i].w);
			if (regions[i].x >= x+w) n = MIN(n, regions[i].x);
			if (regions[i].x + regions[i].w >= x) n = MIN(n, regions[i].x + regions[i].w + w);
			if (regions[i].x >= x) n = MIN(n, regions[i].x + w);
		}
		x = n-w;
	}
	client_commit(c);
	client_moveresize(c, 0, x, y, w, h);
	free(regions);
	winlist_free(visible);
}

// make a window take up 2/3 of a monitor
void client_toggle_large(client *c, int side)
{
	int vague = MAX(c->monitor.w/100, c->monitor.h/100);
	int width3  = c->monitor.w - c->monitor.w/3;
	int height4 = c->monitor.h;

	int is_largeleft  = c->is_left  && c->is_maxv && NEAR(width3, vague, c->w) ?1:0;
	int is_largeright = c->is_right && c->is_maxv && NEAR(width3, vague, c->w) ?1:0;

	c->cache->hlock = 0; c->cache->vlock = 0;
	client_remove_state(c, netatoms[_NET_WM_STATE_MAXIMIZED_HORZ]);

	if (side == LARGELEFT)
	{
		// act like a toggle
		if (is_largeleft)
			client_rollback(c);
		else {
			client_commit(c);
			client_moveresize(c, 0, c->monitor.x, c->monitor.y, width3, height4);
		}
	}
	else
	if (side == LARGERIGHT)
	{
		// act like a toggle
		if (is_largeright)
			client_rollback(c);
		else {
			client_commit(c);
			client_moveresize(c, 0, c->monitor.x + c->monitor.w - width3, c->monitor.y, width3, height4);
		}
	}
	if (!is_largeleft && !is_largeright)
	{
		client_add_state(c, netatoms[_NET_WM_STATE_MAXIMIZED_VERT]);
		client_flash(c, config_flash_on, config_flash_ms, FLASHTITLEDEF);
	}
}

// visually highlight a client to attract attention
// for now, four coloured squares in the corners. could get fancier?
void client_flash(client *c, char *color, int delay, int title)
{
	XSync(display, False);
	if (!fork())
	{
		Window win = c->window;
		display = XOpenDisplay(0x0);
		reset_cache_xattr();
		reset_cache_client();
		reset_cache_inplay();

		c = client_create(win);
		if (!c) exit(EXIT_FAILURE);
		client_descriptive_data(c);
		client_extended_data(c);

		int x1 = c->x, x2 = c->x + c->w - config_flash_width;
		int y1 = c->y, y2 = c->y + c->h - config_flash_width;

		// use message_box for title
		if (title || config_flash_title)
			message_box(delay, c->x+c->w/2, c->y+c->h/2, config_title_fg, config_title_bg, config_title_bc, c->title);

		// four coloured squares in the window's corners
		box *tl = box_create(root, BOX_OVERRIDE, x1, y1, config_flash_width, config_flash_width, color);
		box *tr = box_create(root, BOX_OVERRIDE, x2, y1, config_flash_width, config_flash_width, color);
		box *bl = box_create(root, BOX_OVERRIDE, x1, y2, config_flash_width, config_flash_width, color);
		box *br = box_create(root, BOX_OVERRIDE, x2, y2, config_flash_width, config_flash_width, color);

		box_show(tl); box_show(tr); box_show(bl); box_show(br);

		XSync(display, False);
		usleep(delay*1000);

		box_free(tl); box_free(tr); box_free(bl); box_free(br);

		exit(EXIT_SUCCESS);
	}
}

// add a window and family to the stacking order
void client_stack_family(client *c, winlist *stack)
{
	int i; Window w; client *a = NULL;
	// if this is a transient window, find the main app
	if (c->trans && winlist_find(stack, c->trans) < 0)
	{
		a = client_create(c->trans);
		client_stack_family(a, stack);
		return;
	}
	// make sure this window does not trigger recursion
	winlist_append(stack, c->window, NULL);
	// locate all visible transient windows for this app
	managed_descend(i, w, a)
		if (winlist_find(stack, w) < 0 && a->trans == c->window)
			client_stack_family(a, stack);
	// move this window to end (bottom) of stack
	winlist_forget(stack, c->window);
	winlist_append(stack, c->window, NULL);
	if (c->decorate) winlist_append(stack, c->cache->frame->window, NULL);
}

// raise a window and its transients
void client_raise(client *c, int priority)
{
	int i; Window w; client *o;

	if (!priority && client_has_state(c, netatoms[_NET_WM_STATE_BELOW]))
		return;

	winlist *stack = winlist_new();

	// priority gets us raised without anyone above us, regardless. eg _NET_WM_STATE_FULLSCREEN+focus
	if (!priority)
	{
		// if we're above, ensure it
		// allows cycling between multiple _NET_WM_STATE_ABOVE windows, regardless of their original mapping order
		if (client_has_state(c, netatoms[_NET_WM_STATE_ABOVE]))
			client_stack_family(c, stack);

		// locate windows with both _NET_WM_STATE_STICKY and _NET_WM_STATE_ABOVE
		managed_descend(i, w, o)
			if (winlist_find(stack, w) < 0 && o->visible && o->trans == None
				&& client_has_state(o, netatoms[_NET_WM_STATE_ABOVE])
				&& client_has_state(o, netatoms[_NET_WM_STATE_STICKY]))
					client_stack_family(o, stack);
		// locate windows in the current_tag with _NET_WM_STATE_ABOVE
		tag_descend(i, w, o, current_tag)
			if (winlist_find(stack, w) < 0 && o->visible && o->trans == None
				&& client_has_state(o, netatoms[_NET_WM_STATE_ABOVE]))
					client_stack_family(o, stack);
		// locate _NET_WM_WINDOW_TYPE_DOCK windows
		clients_descend(windows_in_play(), i, w, o)
			if (winlist_find(stack, w) < 0 && o->visible && c->trans == None
				&& o->type == netatoms[_NET_WM_WINDOW_TYPE_DOCK])
					client_stack_family(o, stack);
	}
	// locate our family
	if (winlist_find(stack, c->window) < 0)
		client_stack_family(c, stack);

	// raise the top window in the stack
	XRaiseWindow(display, stack->array[0]);
	// stack everything else, in order, underneath top window
	if (stack->len > 1) XRestackWindows(display, stack->array, stack->len);

	winlist_free(stack);
}

// raise a window and its transients, under someone else
void client_raise_under(client *c, client *under)
{
	if (client_has_state(c, netatoms[_NET_WM_STATE_BELOW]))
		return;

	winlist *stack = winlist_new();
	client_stack_family(under, stack);
	client_stack_family(c, stack);

	// stack everything, in order, underneath top window
	XRestackWindows(display, stack->array, stack->len);

	winlist_free(stack);
}

// lower a window and its transients
void client_lower(client *c, int priority)
{
	int i; Window w; client *o;

	if (!priority && client_has_state(c, netatoms[_NET_WM_STATE_ABOVE]))
		return;

	// locate the lowest window in the tag
	client *under = NULL;
	tag_descend(i, w, o, current_tag)
		if (o->trans == None && o->window != c->window
			&& !client_has_state(o, netatoms[_NET_WM_STATE_BELOW]))
				under = o;

	if (under) client_raise_under(c, under);
}

// set border width approriate to position and size
void client_review_border(client *c)
{
	client_extended_data(c);
	XSetWindowBorderWidth(display, c->window, 0);
	unsigned long extents[4] = { c->border_width, c->border_width, c->border_width + c->titlebar_height, c->border_width };

	if (client_has_state(c, netatoms[_NET_WM_STATE_FULLSCREEN]))
	{
		if (c->cache->frame) box_hide(c->cache->frame);
		memset(extents, 0, sizeof(extents));
	}
	else
	if (c->decorate)
	{
		Window wins[2] = { c->window, c->cache->frame->window };
		XRestackWindows(display, wins, 2);
		if (c->visible) box_show(c->cache->frame);
	}
	window_set_cardinal_prop(c->window, netatoms[_NET_FRAME_EXTENTS], extents, 4);
}

// set allowed _NET_WM_STATE_* client messages
void client_review_nws_actions(client *c)
{
	Atom allowed[7] = {
		netatoms[_NET_WM_ACTION_MOVE],
		netatoms[_NET_WM_ACTION_RESIZE],
		netatoms[_NET_WM_ACTION_FULLSCREEN],
		netatoms[_NET_WM_ACTION_CLOSE],
		netatoms[_NET_WM_ACTION_STICK],
		netatoms[_NET_WM_ACTION_MAXIMIZE_HORZ],
		netatoms[_NET_WM_ACTION_MAXIMIZE_VERT],
	};
	window_set_atom_prop(c->window, netatoms[_NET_WM_ALLOWED_ACTIONS], allowed, 7);
}

// if client is in a screen corner, track it...
// if we shrink the window form maxv/maxh/fullscreen later, we can
// have it stick to the original corner rather then re-centering
void client_review_position(client *c)
{
	if (c->cache && !c->is_full)
	{
		// don't change last_corner if it still matches
		if (!c->cache->last_corner && c->is_xcenter && c->is_ycenter) return;
		if (c->cache->last_corner == CENTERLEFT   && c->is_left   && c->is_ycenter) return;
		if (c->cache->last_corner == CENTERRIGHT  && c->is_right  && c->is_ycenter) return;
		if (c->cache->last_corner == CENTERTOP    && c->is_top    && c->is_xcenter) return;
		if (c->cache->last_corner == CENTERBOTTOM && c->is_bottom && c->is_xcenter) return;
		if (c->cache->last_corner == TOPLEFT      && c->is_left   && c->is_top)     return;
		if (c->cache->last_corner == BOTTOMLEFT   && c->is_left   && c->is_bottom)  return;
		if (c->cache->last_corner == TOPRIGHT     && c->is_right  && c->is_top)     return;
		if (c->cache->last_corner == BOTTOMRIGHT  && c->is_right  && c->is_bottom)  return;
		// nope, we've moved too much. decide on a new corner, preferring left and top
		     if (c->is_left   && c->is_ycenter) c->cache->last_corner = CENTERLEFT;
		else if (c->is_right  && c->is_ycenter) c->cache->last_corner = CENTERRIGHT;
		else if (c->is_top    && c->is_xcenter) c->cache->last_corner = CENTERTOP;
		else if (c->is_bottom && c->is_xcenter) c->cache->last_corner = CENTERBOTTOM;
		else if (c->is_left   && c->is_top)     c->cache->last_corner = TOPLEFT;
		else if (c->is_left   && c->is_bottom)  c->cache->last_corner = BOTTOMLEFT;
		else if (c->is_right  && c->is_top)     c->cache->last_corner = TOPRIGHT;
		else if (c->is_right  && c->is_bottom)  c->cache->last_corner = BOTTOMRIGHT;
		else c->cache->last_corner = 0;
	}
}

// check a window's _NET_WM_DESKTOP. if found, tag it appropriately
void client_review_desktop(client *c)
{
	unsigned long d;
	// no desktop set. give it one
	if (!window_get_cardinal_prop(c->window, netatoms[_NET_WM_DESKTOP], &d, 1))
	{
		d = tag_to_desktop(c->cache->tags);
		window_set_cardinal_prop(c->window, netatoms[_NET_WM_DESKTOP], &d, 1);
	}
	else
	// window has a desktop set. convert it to tag
	if (d < TAGS)
		c->cache->tags |= desktop_to_tag(d);
	else
	if (d == 0xffffffff)
		c->cache->tags = 0;
}

// if client is new or has changed state since we last looked, tweak stuff
void client_full_review(client *c)
{
	client_review_border(c);
	client_review_nws_actions(c);
	client_review_position(c);
	client_review_desktop(c);
}

// configure a client's frame color
void client_redecorate(client *c)
{
	if (!c->decorate) return;

	char *border = config_border_blur;
	if (c->urgent) border = config_border_attention;
	if (c->active) border = config_border_focus;

	box_color(c->cache->frame, border);
	box_draw(c->cache->frame);

	if (!c->titlebar_height) return;
	client_descriptive_data(c);

	textbox_text(c->cache->title, c->title);
	textbox_moveresize(c->cache->title, 0, c->border_width, c->w, c->titlebar_height);

	textbox_font(c->cache->title, config_titlebar_font,
		c->active ? config_titlebar_focus: config_titlebar_blur,
		c->active ? config_border_focus  : config_border_blur);

	textbox_draw(c->cache->title);
}

// update client border to blurred
void client_deactivate(client *c, client *a)
{
	int was_active = c->active;

	c->active = 0;
	client_redecorate(c);

	if (was_active && c->visible && client_rule(c, (RULE_AUTOMINI|RULE_AUTOLOWER)))
	{
		bool trans = 0;
		// check whether the active window is one of our family
		while (a && !trans && a->trans != None)
		{
			if (a->trans == c->window) trans = 1;
			a = client_create(a->trans);
		}
		if (!trans)
		{
			client_lower(c, 0);
			winlist_forget(windows_activated, c->window);
			winlist_prepend(windows_activated, c->window, NULL);
			if (client_rule(c, RULE_AUTOMINI)) client_minimize(c);
		}
	}
}

// raise and focus a client
void client_activate(client *c, int raise, int warp)
{
	int i; Window w; client *o;

	// deactivate everyone else
	clients_ascend(windows_in_play(), i, w, o) if (w != c->window) client_deactivate(o, c);

	if (c->minimized) client_restore(c);
	if (c->shaded) client_reveal(c);

	// setup ourself
	if (config_raise_mode == RAISEFOCUS || raise)
		client_raise(c, client_has_state(c, netatoms[_NET_WM_STATE_FULLSCREEN]));

	// focus a window politely if possible
	client_protocol_event(c, atoms[WM_TAKE_FOCUS]);
	//if (c->input) XSetInputFocus(display, c->window, RevertToPointerRoot, CurrentTime);
	//else XSetInputFocus(display, PointerRoot, RevertToPointerRoot, CurrentTime);
	XSetInputFocus(display, c->window, RevertToPointerRoot, CurrentTime);

	// we have recieved attention
	client_remove_state(c, netatoms[_NET_WM_STATE_DEMANDS_ATTENTION]);
	c->urgent = 0;

	// tell the user something happened
	if (!c->active && !c->trans)
		client_flash(c, config_border_focus, config_flash_ms, FLASHTITLEDEF);

	// update focus history order
	winlist_forget(windows_activated, c->window);
	winlist_append(windows_activated, c->window, NULL);
	ewmh_active_window(c->window);
	c->active = 1;

	// must happen last, after all move/resize/focus/raise stuff is sent
	if (config_warp_mode == WARPFOCUS || warp)
		client_warp_pointer(c);

	// set focus border color
	client_redecorate(c);
}

// set WM_STATE
void client_set_wm_state(client *c, unsigned long state)
{
	unsigned long payload[] = { state, None };
	XChangeProperty(display, c->window, atoms[WM_STATE], atoms[WM_STATE], 32, PropModeReplace, (unsigned char*)payload, 2);
}

unsigned long client_get_wm_state(client *c)
{
	unsigned long payload[2]; int items; Atom type;
	return window_get_prop(c->window, atoms[WM_STATE], &type, &items, payload, 2*sizeof(unsigned long)) && type == atoms[WM_STATE] && items > 0 ? payload[0]: 0;
}

// locate the currently focused window and build a client for it
client* client_active(unsigned int tag)
{
	int i; Window w; client *c = NULL, *o;
	// look for a visible, previously activated window in the current tag
	if (tag) clients_descend(windows_activated, i, w, o)
		if (o->manage && o->visible && o->cache->tags & tag) { c = o; break; }
	// look for a visible, previously activated window anywhere
	if (!c) clients_descend(windows_activated, i, w, o)
		if (o->manage && o->visible) { c = o; break; }
	// otherwise look for any visible, manageable window
	if (!c) managed_descend(i, w, o) { c = o; break; }
	// if we found one, activate it
	if (c && (!c->focus || !c->active))
		client_activate(c, RAISEDEF, WARPDEF);
	return c;
}

// horizontal and vertical window size locking
void client_toggle_vlock(client *c)
{
	c->cache->vlock = c->cache->vlock ? 0:1;
	client_flash(c, c->cache->vlock ? config_flash_on: config_flash_off, config_flash_ms, FLASHTITLEDEF);
}
void client_toggle_hlock(client *c)
{
	c->cache->hlock = c->cache->hlock ? 0:1;
	client_flash(c, c->cache->hlock ? config_flash_on: config_flash_off, config_flash_ms, FLASHTITLEDEF);
}

// go fullscreen on current monitor
void client_nws_fullscreen(client *c, int action)
{
	int state = client_has_state(c, netatoms[_NET_WM_STATE_FULLSCREEN]);

	if (action == ADD || (action == TOGGLE && !state))
	{
		c->cache->hlock = 0;
		c->cache->vlock = 0;
		client_commit(c);
		client_save_position(c);
		client_add_state(c, netatoms[_NET_WM_STATE_FULLSCREEN]);
		c->border_width = 0;
		// _NET_WM_STATE_FULLSCREEN will override size
		client_moveresize(c, 0, c->x, c->y, c->w, c->h);
	}
	else
	if (action == REMOVE || (action == TOGGLE && state))
	{
		client_commit(c);
		client_remove_state(c, netatoms[_NET_WM_STATE_FULLSCREEN]);
		c = client_recreate(c->window);
		client_restore_position(c, 0, c->monitor.x + (c->monitor.w/4), c->monitor.y + (c->monitor.h/4), c->monitor.w/2, c->monitor.h/2);
	}
	// fullscreen may need to hide above windows
	if (c->active) client_activate(c, RAISE, WARPDEF);
}

// raise above other windows
void client_nws_above(client *c, int action)
{
	int state = client_has_state(c, netatoms[_NET_WM_STATE_ABOVE]);
	client_remove_state(c, netatoms[_NET_WM_STATE_BELOW]);

	if (action == ADD || (action == TOGGLE && !state))
	{
		client_add_state(c, netatoms[_NET_WM_STATE_ABOVE]);
		client_raise(c, 0);
		client_flash(c, config_flash_on, config_flash_ms, FLASHTITLEDEF);
	}
	else
	if (action == REMOVE || (action == TOGGLE && state))
	{
		client_remove_state(c, netatoms[_NET_WM_STATE_ABOVE]);
		client_flash(c, config_flash_off, config_flash_ms, FLASHTITLEDEF);
	}
}

// lower below other windows
void client_nws_below(client *c, int action)
{
	int state = client_has_state(c, netatoms[_NET_WM_STATE_BELOW]);
	client_remove_state(c, netatoms[_NET_WM_STATE_ABOVE]);

	if (action == ADD || (action == TOGGLE && !state))
	{
		client_add_state(c, netatoms[_NET_WM_STATE_BELOW]);
		client_lower(c, 0);
		client_flash(c, config_flash_on, config_flash_ms, FLASHTITLEDEF);
	}
	else
	if (action == REMOVE || (action == TOGGLE && state))
	{
		client_remove_state(c, netatoms[_NET_WM_STATE_BELOW]);
		client_flash(c, config_flash_off, config_flash_ms, FLASHTITLEDEF);
	}
}

// stick to screen
void client_nws_sticky(client *c, int action)
{
	int state = client_has_state(c, netatoms[_NET_WM_STATE_STICKY]);

	if (action == ADD || (action == TOGGLE && !state))
	{
		client_add_state(c, netatoms[_NET_WM_STATE_STICKY]);
		client_raise(c, 0);
		client_flash(c, config_flash_on, config_flash_ms, FLASHTITLEDEF);
	}
	else
	if (action == REMOVE || (action == TOGGLE && state))
	{
		client_remove_state(c, netatoms[_NET_WM_STATE_STICKY]);
		client_flash(c, config_flash_off, config_flash_ms, FLASHTITLEDEF);
	}
}

// maximize vertically
void client_nws_maxvert(client *c, int action)
{
	client_extended_data(c);
	int state = client_has_state(c, netatoms[_NET_WM_STATE_MAXIMIZED_VERT]);

	if (action == ADD || (action == TOGGLE && !state))
	{
		c->cache->vlock = 0;
		client_commit(c);
		client_save_position_vert(c);
		client_add_state(c, netatoms[_NET_WM_STATE_MAXIMIZED_VERT]);
		client_moveresize(c, MR_SMART, c->x, c->y, c->w, c->monitor.h);
		client_flash(c, config_flash_on, config_flash_ms, FLASHTITLEDEF);
	}
	else
	if (action == REMOVE || (action == TOGGLE && state))
	{
		client_commit(c);
		client_remove_state(c, netatoms[_NET_WM_STATE_MAXIMIZED_VERT]);
		client_restore_position_vert(c, 0, c->monitor.y + (c->monitor.h/4), c->monitor.h/2);
		client_flash(c, config_flash_off, config_flash_ms, FLASHTITLEDEF);
	}
}

// maximize horizontally
void client_nws_maxhorz(client *c, int action)
{
	client_extended_data(c);
	int state = client_has_state(c, netatoms[_NET_WM_STATE_MAXIMIZED_HORZ]);

	if (action == ADD || (action == TOGGLE && !state))
	{
		c->cache->hlock = 0;
		client_commit(c);
		client_save_position_horz(c);
		client_add_state(c, netatoms[_NET_WM_STATE_MAXIMIZED_HORZ]);
		client_moveresize(c, MR_SMART, c->x, c->y, c->monitor.w, c->h);
		client_flash(c, config_flash_on, config_flash_ms, FLASHTITLEDEF);
	}
	else
	if (action == REMOVE || (action == TOGGLE && state))
	{
		client_commit(c);
		client_remove_state(c, netatoms[_NET_WM_STATE_MAXIMIZED_HORZ]);
		client_restore_position_horz(c, 0, c->monitor.x + (c->monitor.w/4), c->monitor.w/2);
		client_flash(c, config_flash_off, config_flash_ms, FLASHTITLEDEF);
	}
}

// look for windows tiled horizontally with *c
winlist* clients_tiled_horz_with(client *c)
{
	client_extended_data(c);
	int i; Window w; client *o;
	winlist *tiles = winlist_new();
	winlist_append(tiles, c->window, NULL);
	int vague = MAX(c->monitor.w/100, c->monitor.h/100);
	int min_x = c->x, max_x = c->x + c->w, tlen = 0;
	while (tlen != tiles->len)
	{
		tlen = tiles->len;
		tag_descend(i, w, o, c->cache->tags)
		{
			// window is not already found, and is on the same horizontal alignment
			if (c->window != w && winlist_find(tiles, w) < 0 && NEAR(c->y, vague, o->y)
				// window has roughly the same width and height, and aligned with a known left/right edge
				&& NEAR(c->w, vague, o->w) && NEAR(c->h, vague, o->h) && (NEAR(min_x, vague, o->x+o->w) || NEAR(max_x, vague, o->x)))
					{ winlist_append(tiles, w, NULL); min_x = MIN(o->x, min_x); max_x = MAX(o->x+o->w, max_x); }
		}
	}
	return tiles;
}

// look for windows tiled vertically with *c
winlist* clients_tiled_vert_with(client *c)
{
	client_extended_data(c);
	int i; Window w; client *o;
	winlist *tiles = winlist_new();
	winlist_append(tiles, c->window, NULL);
	int vague = MAX(c->monitor.w/100, c->monitor.h/100);
	int min_y = c->y, max_y = c->y + c->h, tlen = 0;
	// locate adjacent windows with the same tag, size, and vertical position
	while (tlen != tiles->len)
	{
		tlen = tiles->len;
		tag_descend(i, w, o, c->cache->tags)
			// window is not already found, and is on the same vertical alignment
			if (c->window != w && winlist_find(tiles, w) < 0 && NEAR(c->x, vague, o->x)
				// window has roughly the same width and height, and aligned with a known top/bottom edge
				&& NEAR(c->w, vague, o->w) && NEAR(c->h, vague, o->h) && (NEAR(min_y, vague, o->y+o->h) || NEAR(max_y, vague, o->y)))
					{ winlist_append(tiles, w, NULL); min_y = MIN(o->y, min_y); max_y = MAX(o->y+o->h, max_y); }
	}
	return tiles;
}

// look for windows tiled with *c
winlist* clients_tiled_with(client *c)
{
	int i, j; Window w, ww; client *o;
	winlist *tiles = clients_tiled_horz_with(c);
	clients_ascend(tiles, i, w, o)
	{
		winlist *vtiles = clients_tiled_vert_with(o);
		winlist_ascend(vtiles, j, ww) if (winlist_find(tiles, ww) < 0)
			winlist_append(tiles, ww, NULL);
		winlist_free(vtiles);
	}
	return tiles;
}

// extend client_acivate() behavior to work with groups of tiled windows
void client_switch_to(client *c)
{
	int i; Window w; client *o;

	// smart tile mode detects windows tiled with the client and treats the whole group as one
	if (config_tile_mode == TILESMART)
	{
		winlist *tiles = clients_tiled_with(c);
		clients_ascend(tiles, i, w, o) if (o->window != c->window)
			client_activate(o, RAISE, WARPDEF);
		winlist_free(tiles);
	}

	client_activate(c, RAISE, WARPDEF);
}

// cycle through tag windows in roughly the same screen position and tag
void client_cycle(client *c)
{
	int i; Window w; client *o;

	// find an intersecting client near the bottom of the stack to raise
	tag_ascend(i, w, o, current_tag)
		if (w != c->window && clients_intersect(c, o))
			{ client_switch_to(o); return; }

	tag_ascend(i, w, o, c->cache->tags)
		if (w != c->window && clients_intersect(c, o))
			{ client_switch_to(o); return; }

	// nothing to cycle. do something visual to acknowledge key press
	client_flash(c, config_border_focus, config_flash_ms, FLASHTITLEDEF);
}

// horizontally tile two windows in the same screen position and tag
void client_htile(client *c)
{
	client_extended_data(c);
	winlist *tiles = winlist_new();
	winlist_append(tiles, c->window, NULL);
	int i, vague = MAX(c->monitor.w/100, c->monitor.h/100); Window w; client *o;
	// locate windows with same tag, size, and position
	tag_descend(i, w, o, current_tag|c->cache->tags) if (c->window != w)
		if (NEAR(c->x, vague, o->x) && NEAR(c->y, vague, o->y) && NEAR(c->w, vague, o->w) && NEAR(c->h, vague, o->h))
			winlist_append(tiles, w, NULL);
	if (tiles->len > 1)
	{
		int width = c->w / tiles->len;
		clients_ascend(tiles, i, w, o)
		{
			client_commit(o);
			client_remove_state(o, netatoms[_NET_WM_STATE_MAXIMIZED_HORZ]);
			client_moveresize(o, 0, c->x+(width*i), c->y, width, c->h);
		}
	}
	winlist_free(tiles);
}

// find windows tiled horizontally and restack them
void client_huntile(client *c)
{
	int i; Window w; client *o;
	client_extended_data(c);
	winlist *tiles = clients_tiled_horz_with(c);
	if (tiles->len > 1)
	{
		int min_x = c->x, max_x = c->x+c->h;
		clients_ascend(tiles, i, w, o)
		{
			client_extended_data(o);
			min_x = MIN(min_x, o->x);
			max_x = MAX(max_x, o->x+o->w);
		}
		clients_ascend(tiles, i, w, o)
		{
			client_commit(o);
			client_remove_state(o, netatoms[_NET_WM_STATE_MAXIMIZED_HORZ]);
			client_moveresize(o, 0, min_x, c->y, max_x-min_x, c->h);
		}
	}
	winlist_free(tiles);
}

// vertically tile two windows in the same screen position and tag
void client_vtile(client *c)
{
	client_extended_data(c);
	winlist *tiles = winlist_new();
	winlist_append(tiles, c->window, NULL);
	int i, vague = MAX(c->monitor.w/100, c->monitor.h/100); Window w; client *o;
	// locate windows with same tag, size, and position
	tag_descend(i, w, o, current_tag|c->cache->tags) if (c->window != w)
		if (NEAR(c->x, vague, o->x) && NEAR(c->y, vague, o->y) && NEAR(c->w, vague, o->w) && NEAR(c->h, vague, o->h))
			winlist_append(tiles, w, NULL);
	if (tiles->len > 1)
	{
		int height = c->h / tiles->len;
		clients_ascend(tiles, i, w, o)
		{
			client_commit(o);
			client_remove_state(o, netatoms[_NET_WM_STATE_MAXIMIZED_VERT]);
			client_moveresize(o, 0, c->x, c->y+(height*i), c->w, height);
		}
	}
	winlist_free(tiles);
}

// find windows tiled vertically and restack them
void client_vuntile(client *c)
{
	int i; Window w; client *o;
	client_extended_data(c);
	winlist *tiles = clients_tiled_vert_with(c);
	if (tiles->len > 1)
	{
		int min_y = c->y, max_y = c->y+c->h;
		clients_ascend(tiles, i, w, o)
		{
			client_extended_data(o);
			min_y = MIN(min_y, o->y);
			max_y = MAX(max_y, o->y+o->h);
		}
		clients_ascend(tiles, i, w, o)
		{
			client_commit(o);
			client_remove_state(o, netatoms[_NET_WM_STATE_MAXIMIZED_HORZ]);
			client_moveresize(o, 0, c->x, min_y, c->w, max_y-min_y);
		}
	}
	winlist_free(tiles);
}

// find client by direction. this is a visual thing
client* client_over_there_ish(client *c, int direction)
{
	client_extended_data(c);
	int i, large = 10000; Window w; client *o;

	workarea zone; memset(&zone, 0, sizeof(workarea));
	if (direction == FOCUSLEFT)
		{ zone.x = 0-large; zone.y = 0-large; zone.w = large + c->x + c->w/2; zone.h = large*2; }
	if (direction == FOCUSRIGHT)
		{ zone.x = c->x+c->w/2; zone.y = 0-large; zone.w = c->w/2 + large; zone.h = large*2; }
	if (direction == FOCUSUP)
		{ zone.x = 0-large; zone.y = 0-large; zone.w = large*2; zone.h = large + c->y + c->h/2; }
	if (direction == FOCUSDOWN)
		{ zone.x = 0-large; zone.y = c->y+c->h/2; zone.w = large*2; zone.h = c->h/2 + large; }

	winlist *consider = clients_partly_visible(&zone, current_tag|c->cache->tags, None);

	client *m = NULL;
	// client that overlaps preferred
	clients_descend(consider, i, w, o) if (w != c->window && o->manage)
	{
		client_extended_data(o);
		int overlap_x = OVERLAP(c->y, c->h, o->y, o->h);
		int overlap_y = OVERLAP(c->x, c->w, o->x, o->w);
		if (direction == FOCUSLEFT  && overlap_x && (!m || o->x+o->w/2 > m->x+m->w/2)) m = o;
		if (direction == FOCUSRIGHT && overlap_x && (!m || o->x+o->w/2 < m->x+m->w/2)) m = o;
		if (direction == FOCUSUP    && overlap_y && (!m || o->y+o->h/2 > m->y+m->h/2)) m = o;
		if (direction == FOCUSDOWN  && overlap_y && (!m || o->y+o->h/2 < m->y+m->h/2)) m = o;
	}
	// otherwise, the closest one
	if (!m) clients_descend(consider, i, w, o) if (w != c->window && o->manage)
	{
		client_extended_data(o);
		if (!m) m = o;
		if (direction == FOCUSLEFT  && o->x+o->w/2 > m->x+m->w/2) m = o;
		if (direction == FOCUSRIGHT && o->x+o->w/2 < m->x+m->w/2) m = o;
		if (direction == FOCUSUP    && o->y+o->h/2 > m->y+m->h/2) m = o;
		if (direction == FOCUSDOWN  && o->y+o->h/2 < m->y+m->h/2) m = o;
	}
	winlist_free(consider);
	return m;
}

// switch focus by direction
void client_focusto(client *c, int direction)
{
	client *match = client_over_there_ish(c, direction);
	if (match) client_activate(match, RAISEDEF, WARPDEF);
}

// swap client position with another by direction
void client_swapto(client *c, int direction)
{
	client_extended_data(c);
	client *m = client_over_there_ish(c, direction);
	// limit to the same monitor. gets weird otherwise...
	if (m && c->monitor.x == m->monitor.x && c->monitor.y == m->monitor.y)
	{
		client_extended_data(m);
		int cx = c->x, cy = c->y, cw = c->w, ch = c->h, mx = m->x, my = m->y, mw = m->w, mh = m->h;
		int overlap_x = OVERLAP(c->y, c->h, m->y, m->h);
		int overlap_y = OVERLAP(c->x, c->w, m->x, m->w);

		if (((direction == FOCUSLEFT || direction == FOCUSRIGHT) && overlap_x) ||
			((direction == FOCUSUP  || direction == FOCUSDOWN ) && overlap_y))
		{
			client_commit(c); client_commit(m);

			// swap EWMH states
			Atom state[CLIENTSTATE]; int states = c->states;
			memmove(&state, c->state,     sizeof(state));
			memmove(&c->state, &m->state, sizeof(state));
			memmove(&m->state, &state,    sizeof(state));
			c->states = m->states; m->states = states;
			client_flush_state(c); client_flush_state(m);

			// swap positions
			client_moveresize(c, 0, mx, my, mw, mh);
			client_moveresize(m, 0, cx, cy, cw, ch);

			// ensure we can be seen
			client_raise(c, 0);
			client_raise_under(m, c);
		}
	}
}

// place a window over the active window
void client_replace(client *c)
{
	client *a = client_active(0);
	if (a)
	{
		client_commit(c);
		client_moveresize(c, 0, a->x, a->y, a->w, a->h);
	}
}

// resize window to match the one underneath
void client_duplicate(client *c)
{
	int i; Window w; client *o; client_commit(c);
	tag_descend(i, w, o, 0)
		if (c->window != w && clients_intersect(c, o))
			{ client_moveresize(c, 0, o->x, o->y, o->w, o->h); return; }
}

void client_minimize(client *c)
{
	XUnmapWindow(display, c->window);
	if (c->decorate) box_show(c->cache->frame);
	// no update fo windows_activated yet. see handle_unmapnotify()
	winlist_forget(windows_minimized, c->window);
	winlist_append(windows_minimized, c->window, NULL);
	client_add_state(c, netatoms[_NET_WM_STATE_HIDDEN]);
	c->minimized = 1; c->visible = 0;

	// also minimize any transients
	int i; Window w; client *o;
	managed_descend(i, w, o)
		if (o->trans == c->window && o->visible && !o->minimized && !o->shaded)
			client_minimize(o);
}

void client_restore(client *c)
{
	XMapWindow(display, c->window);
	if (c->decorate) box_show(c->cache->frame);
	// no update fo windows_minimized yet. see handle_mapnotify()
	winlist_forget(windows_activated, c->window);
	winlist_prepend(windows_activated, c->window, NULL);
	c->minimized = 0; c->shaded = 0; c->visible = 1;

	// also restore any transients
	int i; Window w; client *o;
	clients_descend(windows_minimized, i, w, o)
		if (o->trans == c->window) client_restore(o);
}

void client_shade(client *c)
{
	XUnmapWindow(display, c->window);
	if (c->decorate) box_hide(c->cache->frame);
	// no update fo windows_activated yet. see handle_unmapnotify()
	winlist_forget(windows_shaded, c->window);
	winlist_append(windows_shaded, c->window, NULL);
	client_add_state(c, netatoms[_NET_WM_STATE_SHADED]);
	c->shaded = 1; c->visible = 0;

	// also shade any transients
	int i; Window w; client *o;
	managed_descend(i, w, o)
		if (o->trans == c->window && o->visible && !o->minimized && !o->shaded)
			client_shade(o);
}

void client_reveal(client *c)
{
	client_restore(c);

	// also restore any transients
	int i; Window w; client *o;
	clients_descend(windows_shaded, i, w, o)
		if (o->trans == c->window) client_restore(o);
}

// built-in window switcher
void client_switcher(unsigned int tag)
{
	// TODO: this whole function is messy. build a nicer solution
	char pattern[50], **list = NULL;
	int i, type, classfield = 0, maxtags = 0, lines = 0, above = 0, sticky = 0, minimized = 0, plen = 0;
	Window w; client *c; winlist *ids = winlist_new();

	// type=0 normal windows
	// type=1 shaded windows
	// type=2 minimized windows
	for (type = 0; type < 3; type++)
	{
		winlist *l = windows_activated;
		if (type == 1) l = windows_shaded;
		if (type == 2) l = windows_minimized;
		// calc widths of wm_class and tag csv fields
		clients_descend(l, i, w, c)
		{
			if (c->manage && (c->visible || c->minimized || c->shaded) && !client_has_state(c, netatoms[_NET_WM_STATE_SKIP_TASKBAR]))
			{
				client_descriptive_data(c);
				if (!tag || (c->cache && c->cache->tags & tag))
				{
					if (c->minimized) minimized = 1;
					if (client_has_state(c, netatoms[_NET_WM_STATE_ABOVE])) above = 1;
					if (client_has_state(c, netatoms[_NET_WM_STATE_STICKY])) sticky = 1;
					int j, t; for (j = 0, t = 0; j < TAGS; j++)
						if (c->cache->tags & (1<<j)) t++;
					maxtags = MAX(maxtags, t);
					classfield = MAX(classfield, strlen(c->class));
					winlist_append(ids, c->window, NULL);
					lines++;
				}
			}
		}
	}
	// truncate silly java WM_CLASS strings
	classfield = MAX(5, MIN(classfield, 14));
	maxtags = MAX(0, (maxtags*2)-1);
	if (above || sticky || minimized) plen = sprintf(pattern, "%%-%ds  ", above+sticky+minimized);
	if (maxtags) plen += sprintf(pattern+plen, "%%-%ds  ", maxtags);
	plen += sprintf(pattern+plen, "%%-%ds   %%s", classfield);
	list = allocate_clear(sizeof(char*) * (lines+1)); lines = 0;
	// build the actual list
	clients_ascend(ids, i, w, c)
	{
		client_descriptive_data(c);
		if (!tag || (c->cache && c->cache->tags & tag))
		{
			char tags[32]; memset(tags, 0, 32);
			int j, l; for (l = 0, j = 0; j < TAGS; j++)
				if (c->cache->tags & (1<<j)) l += sprintf(tags+l, "%d,", j+1);
			if (l > 0) tags[l-1] = '\0';

			char aos[6]; memset(aos, 0, 6);
			if (client_has_state(c, netatoms[_NET_WM_STATE_ABOVE])) strcat(aos, "a");
			if (client_has_state(c, netatoms[_NET_WM_STATE_STICKY])) strcat(aos, "s");
			if (c->minimized) strcat(aos, "m");

			char class[15]; memset(class, 0, 15); int clen = strlen(c->class);
			strncpy(class, c->class, MIN(14, clen)); if (clen > 14) strcpy(class+11, "...");

			char *line = allocate(strlen(c->title) + strlen(tags) + strlen(c->class) + classfield + 50);
			if ((above || sticky || minimized) && maxtags) sprintf(line, pattern, aos, tags, class, c->title);
			else if (maxtags) sprintf(line, pattern, tags, class, c->title);
			else sprintf(line, pattern, class, c->title);
			list[lines++] = line;
		}
	}
	if (!fork())
	{
		display = XOpenDisplay(0);
		XSync(display, True);
		char *input = NULL;
		int n = menu(list, &input, "> ", 1);
		if (n >= 0 && list[n])
			window_send_message(root, ids->array[n], netatoms[_NET_ACTIVE_WINDOW], 2, // 2 = pager
				SubstructureNotifyMask | SubstructureRedirectMask);
		else
		if (input) exec_cmd(input);
		exit(EXIT_SUCCESS);
	}
	for (i = 0; i < lines; i++) free(list[i]);
	free(list); winlist_free(ids);
}

// toggle client in current tag
void client_toggle_tag(client *c, unsigned int tag, int flash)
{
	if (c->cache->tags & tag)
	{
		c->cache->tags &= ~tag;
		if (flash) client_flash(c, config_flash_off, config_flash_ms, FLASHTITLEDEF);
	} else
	{
		c->cache->tags |= tag;
		if (flash) client_flash(c, config_flash_on, config_flash_ms, FLASHTITLEDEF);
	}
	// update _NET_WM_DESKTOP using lowest tag number.
	// this is a bit of a fudge as we can have windows on multiple
	// tags/desktops, without being specifically sticky... oh well.
	unsigned long d = tag_to_desktop(c->cache->tags);
	window_set_cardinal_prop(c->window, netatoms[_NET_WM_DESKTOP], &d, 1);
}

// search for first open window matching class/name/title
client* client_find(char *pattern)
{
	if (!pattern) return None;
	int i; Window w; client *c = NULL, *found = NULL;

	// use a tempoarary rule for searching
	rule_parse(pattern);
	winrule *rule = config_rules;
	config_rules = rule->next;

	// first, try in current_tag only
	tag_descend(i, w, c, current_tag)
		if (client_rule_match(c, rule)) { found = c; break; }
	// look for something minimized or shaded
	if (!found) clients_descend(windows_minimized, i, w, c)
		if (c->cache->tags & current_tag && client_rule_match(c, rule))
			{ found = c; client_restore(c); break; }
	if (!found) clients_descend(windows_shaded, i, w, c)
		if (c->cache->tags & current_tag && client_rule_match(c, rule))
			{ found = c; client_restore(c); break; }
	// failing that, search regardless of tag
	if (!found) managed_descend(i, w, c)
		if (client_rule_match(c, rule)) { found = c; break; }
	if (!found) clients_descend(windows_minimized, i, w, c)
		if (client_rule_match(c, rule)) { found = c; client_restore(c); break; }
	if (!found) clients_descend(windows_shaded, i, w, c)
		if (client_rule_match(c, rule)) { found = c; client_restore(c); break; }

	rule_free(rule);
	return found;
}

// execute a pattern as a shell command
void client_start(char *pattern)
{
	if (regquick("^(class|name|title):", pattern))
		pattern = strchr(pattern, ':')+1;
	exec_cmd(pattern);
}

// search for and activate first open window matching class/name/title
void client_find_or_start(char *pattern)
{
	if (!pattern) return;
	client *c = client_find(pattern);
	if (c) client_switch_to(c);
	else client_start(pattern);
}

void client_rules_ewmh(client *c)
{
	// process EWMH rules
	// above below are mutally exclusize
		if (client_rule(c, RULE_ABOVE)) client_add_state(c, netatoms[_NET_WM_STATE_ABOVE]);
	else if (client_rule(c, RULE_BELOW)) client_add_state(c, netatoms[_NET_WM_STATE_BELOW]);

	// sticky,skip_taskbar,skip_pager can be on anything
	if (client_rule(c, RULE_STICKY))   client_add_state(c, netatoms[_NET_WM_STATE_STICKY]);
	if (client_rule(c, RULE_SKIPTBAR)) client_add_state(c, netatoms[_NET_WM_STATE_SKIP_TASKBAR]);
	if (client_rule(c, RULE_SKIPPAGE)) client_add_state(c, netatoms[_NET_WM_STATE_SKIP_PAGER]);

	// fullscreen overrides max h/v
	if (client_rule(c, RULE_FULLSCREEN))
		client_add_state(c, netatoms[_NET_WM_STATE_FULLSCREEN]);
	else
	// max h/v overrides lock h/v
	if (client_rule(c, RULE_MAXHORZ|RULE_MAXVERT))
	{
		if (client_rule(c, RULE_MAXHORZ)) client_add_state(c, netatoms[_NET_WM_STATE_MAXIMIZED_HORZ]);
		if (client_rule(c, RULE_MAXVERT)) client_add_state(c, netatoms[_NET_WM_STATE_MAXIMIZED_VERT]);
	}
}

void client_rules_monitor(client *c)
{
	if (client_rule(c, RULE_MONITOR1|RULE_MONITOR2|RULE_MONITOR3))
	{
		client_extended_data(c);
		XineramaScreenInfo *info; int monitors;
		workarea mon; memset(&mon, 0, sizeof(workarea));
		if ((info = XineramaQueryScreens(display, &monitors)))
		{
			if (client_rule(c, RULE_MONITOR1) && monitors > 0)
				monitor_dimensions_struts(info[0].x_org+1, info[0].y_org+1, &mon);
			if (client_rule(c, RULE_MONITOR2) && monitors > 1)
				monitor_dimensions_struts(info[1].x_org+1, info[1].y_org+1, &mon);
			if (client_rule(c, RULE_MONITOR3) && monitors > 2)
				monitor_dimensions_struts(info[2].x_org+1, info[2].y_org+1, &mon);
			XFree(info);
		}
		// only move the window if the monitor has changed. this preserves PLACEPOINTER if
		// the target monitor == the active monitor
		if (mon.w && (mon.x != c->monitor.x || mon.y != c->monitor.y))
		{
			memmove(&c->monitor, &mon, sizeof(workarea));
			client_moveresize(c, 0, MAX(mon.x, mon.x + ((mon.w - c->w) / 2)),
				MAX(mon.y, mon.y + ((mon.h - c->h) / 2)), c->w, c->h);
		}
	}
}

void client_rules_moveresize(client *c)
{
	client_extended_data(c);
	int mr = 0;

	// if a size rule exists, apply it
	if (client_rule(c, RULE_SMALL|RULE_MEDIUM|RULE_LARGE|RULE_COVER|RULE_SIZE))
	{
		int w_small = c->monitor.w/3, h_small = c->monitor.h/3;
		if (client_rule(c, RULE_SMALL))  { c->w = w_small; c->h = h_small; }
		if (client_rule(c, RULE_MEDIUM)) { c->w = c->monitor.w/2; c->h = c->monitor.h/2; }
		if (client_rule(c, RULE_LARGE))  { c->w = c->monitor.w-w_small; c->h = c->monitor.h-h_small; }
		if (client_rule(c, RULE_COVER))  { c->w = c->monitor.w; c->h = c->monitor.h; }
		if (client_rule(c, RULE_SIZE))
		{
			c->w = c->rule->w_is_pct ? c->monitor.w/100*c->rule->w: c->rule->w;
			c->h = c->rule->h_is_pct ? c->monitor.h/100*c->rule->h: c->rule->h;
		}
		mr = 1;
	}
	//  if a placement rule exists, it trumps everything
	if (client_rule(c, RULE_TOP|RULE_LEFT|RULE_RIGHT|RULE_BOTTOM|RULE_CENTER|RULE_POINTER))
	{
		c->x = MAX(c->monitor.x, c->monitor.x + ((c->monitor.w - c->w) / 2));
		c->y = MAX(c->monitor.y, c->monitor.y + ((c->monitor.h - c->h) / 2));
		// center first, so others can combine with it
		if (client_rule(c, RULE_CENTER))
		{
			c->x = c->monitor.x + (c->monitor.w - c->w)/2;
			c->y = c->monitor.y + (c->monitor.h - c->h)/2;
		}
		if (client_rule(c, RULE_POINTER))
		{
			int x, y; pointer_get(&x, &y);
			workarea a; monitor_dimensions_struts(x, y, &a);
			c->x = MAX(a.x, x-(c->w/2));
			c->y = MAX(a.y, y-(c->h/2));
		}
		if (client_rule(c, RULE_BOTTOM)) c->y = c->monitor.y + c->monitor.h - c->h;
		if (client_rule(c, RULE_RIGHT))  c->x = c->monitor.x + c->monitor.w - c->w;
		if (client_rule(c, RULE_TOP))    c->y = c->monitor.y;
		if (client_rule(c, RULE_LEFT))   c->x = c->monitor.x;
		mr = 1;
	}
	if (mr) client_moveresize(c, 0, c->x, c->y, c->w, c->h);
}

// h/v lock must occur after the first client_moveresize
void client_rules_locks(client *c)
{
	if (client_rule(c, RULE_HLOCK|RULE_VLOCK))
	{
		if (!client_rule(c, RULE_MAXHORZ) && client_rule(c, RULE_HLOCK)) c->cache->hlock = 1;
		if (!client_rule(c, RULE_MAXVERT) && client_rule(c, RULE_VLOCK)) c->cache->vlock = 1;
	}
}

// apply tags
void client_rules_tags(client *c)
{
	if (client_rule(c, (TAG1|TAG2|TAG3|TAG4|TAG5|TAG6|TAG7|TAG8|TAG9)))
	{
		c->cache->tags = 0;
		client_toggle_tag(c, c->rule->flags & (TAG1|TAG2|TAG3|TAG4|TAG5|TAG6|TAG7|TAG8|TAG9), NOFLASH);
	}
}

// post-placement rules
void client_rules_moveresize_post(client *c)
{
	unsigned int tag = current_tag; current_tag = desktop_to_tag(tag_to_desktop(c->cache->tags));
	if (client_rule(c, RULE_SNAPRIGHT)) client_snapto(c, SNAPRIGHT);
	if (client_rule(c, RULE_SNAPLEFT))  client_snapto(c, SNAPLEFT);
	if (client_rule(c, RULE_SNAPDOWN))  client_snapto(c, SNAPDOWN);
	if (client_rule(c, RULE_SNAPUP))    client_snapto(c, SNAPUP);
	// yes, can do both contract and expand in one rule. it makes sense...
	if (client_rule(c, RULE_CONTRACT))  client_contract(c, HORIZONTAL|VERTICAL);
	if (client_rule(c, RULE_EXPAND))    client_expand(c, HORIZONTAL|VERTICAL, 0, 0, 0, 0, 0, 0, 0, 0);
	if (client_rule(c, RULE_REPLACE))   client_replace(c);
	if (client_rule(c, RULE_DUPLICATE)) client_duplicate(c);
	// tiling
	if (client_rule(c, RULE_HUNTILE)) client_huntile(c);
	if (client_rule(c, RULE_HTILE))   client_htile(c);
	if (client_rule(c, RULE_VUNTILE)) client_vuntile(c);
	if (client_rule(c, RULE_VTILE))   client_vtile(c);
	current_tag = tag;
}

// check and apply all possible rules to a client
void client_rules_apply(client *c, bool reset)
{
	if (client_rule(c, RULE_RESET) || reset)
	{
		client_remove_all_states(c);
		c->cache->vlock = 0;
		c->cache->hlock = 0;
	}

	client_rules_ewmh(c);
	client_rules_monitor(c);
	client_rules_moveresize(c);
	client_rules_locks(c);
	client_rules_tags(c);
	client_rules_moveresize_post(c);

	if (client_rule(c, RULE_LOWER)) client_lower(c, 0);
	if (client_rule(c, RULE_RAISE)) client_raise(c, 0);
	if (client_rule(c, RULE_RESTORE))  client_restore(c);
	if (client_rule(c, RULE_MINIMIZE)) client_minimize(c);
}

#ifdef DEBUG
// debug
void event_client_dump(client *c)
{
	if (!c) return;
	client_descriptive_data(c);
	client_extended_data(c);
	event_note("title: %s", c->title);
	event_note("class: %s name: %s", c->class, c->name);
	event_note("manage:%d input:%d focus:%d initial_state:%d decorate:%d urgent:%d", c->manage, c->input, c->focus, c->initial_state, c->decorate, c->urgent);
	event_note("x:%d y:%d w:%d h:%d b:%d override:%d transient:%x", c->xattr.x, c->xattr.y, c->xattr.width, c->xattr.height,
		c->xattr.border_width, c->xattr.override_redirect ?1:0, (unsigned int)c->trans);
	event_note("is_full:%d is_left:%d is_top:%d is_right:%d is_bottom:%d\n\tis_xcenter:%d is_ycenter:%d is_maxh:%d is_maxv:%d last_corner:%d",
		c->is_full, c->is_left, c->is_top, c->is_right, c->is_bottom, c->is_xcenter, c->is_ycenter, c->is_maxh, c->is_maxv, c->cache->last_corner);
	event_note("PMinSize:%d,%d,%d PMaxSize:%d,%d,%d PBaseSize:%d,%d,%d PResizeInc:%d,%d,%d PAspect:%d,%d/%d,%d/%d",
		(c->xsize.flags & PMinSize ? 1: 0),
			(c->xsize.flags & PMinSize ? c->xsize.min_width: 0),
			(c->xsize.flags & PMinSize ? c->xsize.min_height: 0),
		(c->xsize.flags & PMaxSize ? 1: 0),
			(c->xsize.flags & PMaxSize ? c->xsize.max_width: 0),
			(c->xsize.flags & PMaxSize ? c->xsize.max_height: 0),
		(c->xsize.flags & PBaseSize ? 1: 0),
			(c->xsize.flags & PBaseSize ? c->xsize.base_width: 0),
			(c->xsize.flags & PBaseSize ? c->xsize.base_height: 0),
		(c->xsize.flags & PResizeInc ? 1: 0),
			(c->xsize.flags & PResizeInc ? c->xsize.width_inc: 0),
			(c->xsize.flags & PResizeInc ? c->xsize.height_inc: 0),
		(c->xsize.flags & PAspect ? 1: 0),
			(c->xsize.flags & PAspect ? c->xsize.min_aspect.x: 0),
			(c->xsize.flags & PAspect ? c->xsize.min_aspect.y: 0),
			(c->xsize.flags & PAspect ? c->xsize.max_aspect.x: 0),
			(c->xsize.flags & PAspect ? c->xsize.max_aspect.y: 0));
	event_note("monitor: %d %d %d %d %d %d %d %d",
		c->monitor.x, c->monitor.y, c->monitor.w, c->monitor.h, c->monitor.l, c->monitor.r, c->monitor.t, c->monitor.b);
	int i, j;
	for (i = 0; i < NETATOMS; i++) if (c->type == netatoms[i]) event_note("type:%s", netatom_names[i]);
	for (i = 0; i < NETATOMS; i++) for (j = 0; j < c->states; j++) if (c->state[j] == netatoms[i]) event_note("state:%s", netatom_names[i]);
	unsigned long struts[12];
	if (window_get_cardinal_prop(c->window, netatoms[_NET_WM_STRUT_PARTIAL], struts, 12))
		event_note("strut partial: %d %d %d %d %d %d %d %d %d %d %d %d",
			struts[0],struts[1],struts[2],struts[3],struts[4],struts[5],struts[6],struts[7],struts[8],struts[9],struts[10],struts[11]);
	if (window_get_cardinal_prop(c->window, netatoms[_NET_WM_STRUT], struts, 4))
		event_note("strut: %d %d %d %d",
			struts[0],struts[1],struts[2],struts[3]);
	if (c->rule)
		event_note("rule: %lx", c->rule->flags);
	fflush(stdout);
}
#else
#define event_client_dump(...)
#endif
