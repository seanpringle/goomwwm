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

// MODKEY+keys
void handle_keypress(XEvent *ev)
{
	event_log("KeyPress", ev->xany.window);
	KeySym key = XkbKeycodeToKeysym(display, ev->xkey.keycode, 0, 0);

	int i; client *c = NULL;

	if (key == keymap[KEY_SWITCH])
	{
		if (config_switcher) exec_cmd(config_switcher);
		else client_switcher(ev->xany.window, 0);
	}
	else if (key == keymap[KEY_LAUNCH]) exec_cmd(config_launcher);

	// custom MODKEY launchers
	// on the command line: goomwwm -1 "firefox"
	else if ((i = in_array_keysym(config_apps_keysyms, key)) >= 0)
		client_find_or_start(ev->xany.window, config_apps_patterns[i]);

	else if ((i = in_array_keysym(config_tags_keysyms, key)) >= 0)
		tag_raise(1<<i);

	// tag cycling
	else if (key == keymap[KEY_TAGNEXT]) tag_raise(current_tag & TAG9 ? TAG1: current_tag<<1);
	else if (key == keymap[KEY_TAGPREV]) tag_raise(current_tag & TAG1 ? TAG9: current_tag>>1);

	else
	// following only relevant with a focused window
	if ((c = client_active(ev->xany.window, 0)) && c)
	{
		client_descriptive_data(c);
		client_extended_data(c);

		int screen_x = c->monitor.x, screen_y = c->monitor.y;
		int screen_width = c->monitor.w, screen_height = c->monitor.h;
		int vague = MAX(screen_width/100, screen_height/100);

		// window co-ords translated to 0-based on screen
		int x = c->sx; int y = c->sy; int w = c->sw; int h = c->sh;

		// four basic window sizes
		int width1 = screen_width/3;      int height1 = screen_height/3;
		int width2 = screen_width/2;      int height2 = screen_height/2;
		int width3 = screen_width-width1; int height3 = screen_height-height1;
		int width4 = screen_width;        int height4 = screen_height;

		// final resize/move params. smart = intelligently bump / center / shrink
		int fx = 0, fy = 0, fw = 0, fh = 0, smart = 0;

		     if (key == keymap[KEY_CLOSE])      client_close(c);
		else if (key == keymap[KEY_CYCLE])      client_cycle(c);
		else if (key == keymap[KEY_TAG])        client_toggle_tag(c, current_tag, FLASH);
		else if (key == keymap[KEY_ABOVE])      client_nws_above(c, TOGGLE);
		else if (key == keymap[KEY_BELOW])      client_nws_below(c, TOGGLE);
		else if (key == keymap[KEY_STICKY])     client_nws_sticky(c, TOGGLE);
		else if (key == keymap[KEY_FULLSCREEN]) client_nws_fullscreen(c, TOGGLE);
		else if (key == keymap[KEY_HMAX])       client_nws_maxhorz(c, TOGGLE);
		else if (key == keymap[KEY_VMAX])       client_nws_maxvert(c, TOGGLE);
		else if (key == keymap[KEY_EXPAND])     client_expand(c, HORIZONTAL|VERTICAL, 0, 0, 0, 0, 0, 0, 0, 0);
		else if (key == keymap[KEY_CONTRACT])   client_contract(c, HORIZONTAL|VERTICAL);
		else if (key == keymap[KEY_VLOCK])      client_toggle_vlock(c);
		else if (key == keymap[KEY_HLOCK])      client_toggle_hlock(c);
		else if (key == keymap[KEY_HTILE])      client_htile(c);
		else if (key == keymap[KEY_VTILE])      client_vtile(c);
		else if (key == keymap[KEY_UNDO])       client_rollback(c);
		else if (key == keymap[KEY_DUPLICATE])  client_duplicate(c);
		else if (key == keymap[KEY_INFO])       client_flash(c, config_border_focus, FLASHMSTITLE);

		// directional focus change
		else if (key == keymap[KEY_FOCUSLEFT])  client_focusto(c, FOCUSLEFT);
		else if (key == keymap[KEY_FOCUSRIGHT]) client_focusto(c, FOCUSRIGHT);
		else if (key == keymap[KEY_FOCUSUP])    client_focusto(c, FOCUSUP);
		else if (key == keymap[KEY_FOCUSDOWN])  client_focusto(c, FOCUSDOWN);

		else
		// cycle through windows with same tag
		if (key == keymap[KEY_TSWITCH])
			client_switcher(c->xattr.root, current_tag);
		else
		// Page Up/Down makes the focused window larger and smaller respectively
		if (!client_has_state(c, netatoms[_NET_WM_STATE_FULLSCREEN])
			&& (key == keymap[KEY_GROW] || key == keymap[KEY_SHRINK]))
		{
			smart = 1; fx = screen_x + c->sx; fy = screen_y + c->sy;

			// window width zone
			int isw4 = (w >= width4 || NEAR(width4, vague, w)) ?1:0;
			int isw3 = !isw4 && (w >= width3 || NEAR(width3, vague, w)) ?1:0;
			int isw2 = !isw4 && !isw3 && (w >= width2 || NEAR(width2, vague, w)) ?1:0;
			int isw1 = !isw4 && !isw3 && !isw2 && (w >= width1 || NEAR(width1, vague, w)) ?1:0;
			int widths[7] = { width1, width1, width1, width2, width3, width4, width4 };
			int isw = isw4 ? 5: (isw3 ? 4: (isw2 ? 3: (isw1 ? 2: 1)));

			// window height zone
			int ish4 = (h >= height4 || NEAR(height4, vague, h)) ?1:0;
			int ish3 = !ish4 && (h >= height3 || NEAR(height3, vague, h)) ?1:0;
			int ish2 = !ish4 && !ish3 && (h >= height2 || NEAR(height2, vague, h)) ?1:0;
			int ish1 = !ish4 && !ish3 && !ish2 && (h >= height1 || NEAR(height1, vague, h)) ?1:0;
			int heights[7] = { height1, height1, height1, height2, height3, height4, height4 };
			int ish = ish4 ? 5: (ish3 ? 4: (ish2 ? 3: (ish1 ? 2: 1)));

			if (client_has_state(c, netatoms[_NET_WM_STATE_MAXIMIZED_HORZ]))
			{
				fw = screen_width;
				if (key == keymap[KEY_GROW]) fh = heights[ish+1];
				if (key == keymap[KEY_SHRINK]) fh = heights[ish-1];
			} else
			if (client_has_state(c, netatoms[_NET_WM_STATE_MAXIMIZED_VERT]))
			{
				fh = screen_height;
				if (key == keymap[KEY_GROW]) fw = widths[isw+1];
				if (key == keymap[KEY_SHRINK]) fw = widths[isw-1];
			} else
			{
				int prefer_width = w > h ? 1:0;

				int is4 = (isw4 && ish4) || (isw4 && prefer_width) || (ish4 && !prefer_width) ?1:0;
				int is3 = !is4 && ((isw3 && ish3) || (isw3 && prefer_width) || (ish3 && !prefer_width)) ?1:0;
				int is2 = !is4 && !is3 && ((isw2 && ish2) || (isw2 && prefer_width) || (ish2 && !prefer_width)) ?1:0;
				int is1 = !is4 && !is3 && !is2 && ((isw1 && ish1) || (isw1 && prefer_width) || (ish1 && !prefer_width)) ?1:0;
				int is = is4 ? 5: (is3 ? 4: (is2 ? 3: (is1 ? 2: 1)));

				if (key == keymap[KEY_GROW])   { fw = widths[is+1]; fh = heights[is+1]; }
				if (key == keymap[KEY_SHRINK]) { fw = widths[is-1]; fh = heights[is-1]; }
			}
		}
		else
		// window movement with arrow keys
		{
			workarea mon;
			int wx = x + w/2; int wy = y + h/2;
			int cx = (screen_width  - w) / 2;
			int cy = (screen_height - h) / 2;
			// expire the toggle cache
			c->cache->have_old = 0;

			// monitor switching if window is on an edge
			if (key == keymap[KEY_LEFT] && c->is_left)
			{
				monitor_dimensions_struts(c->xattr.screen, c->monitor.x-c->monitor.l-vague, c->y, &mon);
				if (mon.x < c->monitor.x && !INTERSECT(mon.x, mon.y, mon.w, mon.h, c->monitor.x, c->monitor.y, c->monitor.h, c->monitor.w))
					{ fx = mon.x+mon.w-w; fy = y; fw = w; fh = h; }
			}
			else
			if (key == keymap[KEY_RIGHT] && c->is_right)
			{
				monitor_dimensions_struts(c->xattr.screen, c->monitor.x+c->monitor.w+c->monitor.r+vague, c->y, &mon);
				if (mon.x > c->monitor.x && !INTERSECT(mon.x, mon.y, mon.w, mon.h, c->monitor.x, c->monitor.y, c->monitor.h, c->monitor.w))
					{ fx = mon.x; fy = y; fw = w; fh = h; }
			}
			else
			if (key == keymap[KEY_UP] && c->is_top)
			{
				monitor_dimensions_struts(c->xattr.screen, c->x, c->monitor.y-c->monitor.t-vague, &mon);
				if (mon.y < c->monitor.y && !INTERSECT(mon.x, mon.y, mon.w, mon.h, c->monitor.x, c->monitor.y, c->monitor.h, c->monitor.w))
					{ fx = x; fy = mon.y+mon.h-h; fw = w; fh = h; }
			}
			else
			if (key == keymap[KEY_DOWN] && c->is_bottom)
			{
				monitor_dimensions_struts(c->xattr.screen, c->x, c->monitor.y+c->monitor.h+c->monitor.b+vague, &mon);
				if (mon.y > c->monitor.y && !INTERSECT(mon.x, mon.y, mon.w, mon.h, c->monitor.x, c->monitor.y, c->monitor.h, c->monitor.w))
					{ fx = x; fy = mon.y; fw = w; fh = h; }
			}
			else
			// MODKEY+Arrow movement occurs on a 3x3 grid for non-fullscreen, managed windows
			if (!c->is_full)
			{
				// move within current monitor
				if (key == keymap[KEY_LEFT] && !c->is_maxh)
					{ fx = screen_x + (wx > (screen_width/2)+vague ? cx: 0); fy = screen_y+y; fw = w; fh = h; }
				else
				if (key == keymap[KEY_RIGHT] && !c->is_maxh)
					{ fx = screen_x + (wx < (screen_width/2)-vague ? cx: screen_width - w); fy = screen_y+y; fw = w; fh = h; }
				else
				if (key == keymap[KEY_UP] && !c->is_maxv)
					{ fx = screen_x+x; fy = screen_y + (wy > (screen_height/2)+vague ? cy: 0); fw = w; fh = h; }
				else
				if (key == keymap[KEY_DOWN] && !c->is_maxv)
					{ fx = screen_x+x; fy = screen_y + (wy < (screen_height/2)-vague ? cy: screen_height - h); fw = w; fh = h; }
			}
		}
		// final co-ords are fixed. go to it...
		if (fw > 0 && fh > 0)
		{
			client_commit(c);
			client_moveresize(c, smart, fx, fy, fw, fh);
		}
	}
}

// we bind on all mouse buttons on the root window to implement click-to-focus
// events are compressed, checked for a window change, then replayed through to clients
void handle_buttonpress(XEvent *ev)
{
	event_log("ButtonPress", ev->xbutton.subwindow);
	// all mouse button events except the wheel come here, so we can click-to-focus
	// turn off caps and num locks bits. dont care about their states
	int state = ev->xbutton.state & ~(LockMask|NumlockMask); client *c = NULL;
	int is_mod = state & config_modkey && !(state & config_ignore_modkeys);

	if (ev->xbutton.subwindow != None && (c = client_create(ev->xbutton.subwindow)) && c && c->manage
		&& !client_has_state(c, netatoms[_NET_WM_STATE_FULLSCREEN]))
	{
		if (!c->focus || !c->active) client_activate(c, RAISEDEF, WARPDEF);

		// Mod+Button1 raises a window. this might already have been raised in
		// client_activate(), but doing the restack again is not a big deal
		if (is_mod && ev->xbutton.button == Button1) client_raise(c, 0);

		// moving or resizing
		if (is_mod)
		{
			XGrabPointer(display, c->window, True, PointerMotionMask|ButtonReleaseMask,
				GrabModeAsync, GrabModeAsync, None, None, CurrentTime);
			memcpy(&mouse_attr, &c->xattr, sizeof(c->xattr));
			memcpy(&mouse_button, &ev->xbutton, sizeof(ev->xbutton));
			mouse_dragging = 1;
		} else
		{
			// events we havn't snaffled for move/resize may be relevant to the subwindow. replay them
			XAllowEvents(display, ReplayPointer, CurrentTime);
		}
	}
	// click was on root window
	else
	{
		XSetInputFocus(display, PointerRoot, RevertToPointerRoot, CurrentTime);
		// events we havn't snaffled for move/resize may be relevant to the subwindow. replay them
		XAllowEvents(display, ReplayPointer, CurrentTime);
	}
	event_client_dump(c);
}

// only get these if a window move/resize has been started in buttonpress
void handle_buttonrelease(XEvent *ev)
{
	event_log("ButtonRelease", ev->xbutton.window);
	int state = ev->xbutton.state & ~(LockMask|NumlockMask); client *c = NULL;
	int is_mod = state & config_modkey && !(state & config_ignore_modkeys);

	if (ev->xbutton.window != None && (c = client_create(ev->xbutton.window)) && c && c->manage)
	{
		int xd = ev->xbutton.x_root - mouse_button.x_root;
		int yd = ev->xbutton.y_root - mouse_button.y_root;

		// if no resize or move has occurred, allow Mod+Button3 to lower a window
		if (!xd && !yd && is_mod && ev->xbutton.button == Button3)
			client_lower(c, 0);
	}
	XUngrabPointer(display, CurrentTime);
	mouse_dragging = 0;
}

// only get these if a window move/resize has been started in buttonpress
void handle_motionnotify(XEvent *ev)
{
	// compress events to reduce window jitter and CPU load
	while(XCheckTypedEvent(display, MotionNotify, ev));
	client *c = client_create(ev->xmotion.window);
	if (c && c->manage)
	{
		client_extended_data(c);
		int xd = ev->xbutton.x_root - mouse_button.x_root;
		int yd = ev->xbutton.y_root - mouse_button.y_root;
		int x  = mouse_attr.x + (mouse_button.button == Button1 ? xd : 0);
		int y  = mouse_attr.y + (mouse_button.button == Button1 ? yd : 0);
		int w  = MAX(1, mouse_attr.width  + (mouse_button.button == Button3 ? xd : 0));
		int h  = MAX(1, mouse_attr.height + (mouse_button.button == Button3 ? yd : 0));
		int vague = MAX(c->monitor.w/100, c->monitor.h/100);
		int i; Window win; client *o;
		int xsnap = 0, ysnap = 0, bw = config_border_width*2;

		// horz/vert size locks
		if (c->cache->hlock) { x = c->x; w = c->w; }
		if (c->cache->vlock) { y = c->y; h = c->h; }

		// monitor_dimensions_struts() can be heavy work with mouse events. only do it if necessary
		if (client_has_state(c, netatoms[_NET_WM_STATE_MAXIMIZED_HORZ]) || client_has_state(c, netatoms[_NET_WM_STATE_MAXIMIZED_VERT]))
		{
			int px, py; pointer_get(c->xattr.root, &px, &py);
			workarea mon; monitor_dimensions_struts(c->xattr.screen, px, py, &mon);
			// ensure we match maxv/maxh mode. these override above locks!
			if (client_has_state(c, netatoms[_NET_WM_STATE_MAXIMIZED_HORZ]))
				{ x = mon.x; w = mon.w-bw; }
			if (client_has_state(c, netatoms[_NET_WM_STATE_MAXIMIZED_VERT]))
				{ y = mon.y; h = mon.h-bw; }
		}
		// Button1 = move
		if (mouse_button.button == Button1)
		{
			// snap to monitor edges
			if (NEAR(c->monitor.x, vague, x)) { x = c->monitor.x; xsnap = 1; }
			if (NEAR(c->monitor.y, vague, y)) { y = c->monitor.y; ysnap = 1; }
			if (!xsnap && NEAR(c->monitor.x+c->monitor.w, vague, x+w)) { x = c->monitor.x+c->monitor.w-w-bw; xsnap = 1; }
			if (!ysnap && NEAR(c->monitor.y+c->monitor.h, vague, y+h)) { y = c->monitor.y+c->monitor.h-h-bw; ysnap = 1; }
			// snap to window edges
			if (!xsnap || !ysnap) managed_descend(c->xattr.root, i, win, o) if (win != c->window)
			{
				client_extended_data(o);
				if (!xsnap && NEAR(o->x, vague, x)) { x = o->x; xsnap = 1; }
				if (!ysnap && NEAR(o->y, vague, y)) { y = o->y; ysnap = 1; }
				if (!xsnap && NEAR(o->x+o->sw, vague, x)) { x = o->x+o->sw; xsnap = 1; }
				if (!ysnap && NEAR(o->y+o->sh, vague, y)) { y = o->y+o->sh; ysnap = 1; }
				if (!xsnap && NEAR(o->x, vague, x+w)) { x = o->x+-w-bw; xsnap = 1; }
				if (!ysnap && NEAR(o->y, vague, y+h)) { y = o->y+-h-bw; ysnap = 1; }
				if (!xsnap && NEAR(o->x+o->sw, vague, x+w)) { x = o->x+o->sw-w-bw; xsnap = 1; }
				if (!ysnap && NEAR(o->y+o->sh, vague, y+h)) { y = o->y+o->sh-h-bw; ysnap = 1; }
				if (xsnap && ysnap) break;
			}
		}
		else
		// Button3 = resize
		if (mouse_button.button == Button3)
		{
			// snap to monitor edges
			if (NEAR(c->monitor.x+c->monitor.w, vague, x+w)) { w = c->monitor.x+c->monitor.w-x-bw; xsnap = 1; }
			if (NEAR(c->monitor.y+c->monitor.h, vague, y+h)) { h = c->monitor.y+c->monitor.h-y-bw; ysnap = 1; }
			// snap to window edges
			if (!xsnap || !ysnap) managed_descend(c->xattr.root, i, win, o) if (win != c->window)
			{
				client_extended_data(o);
				if (!xsnap && NEAR(o->x, vague, x+w)) { w = o->x-x-bw; xsnap = 1; }
				if (!ysnap && NEAR(o->y, vague, y+h)) { h = o->y-y-bw; ysnap = 1; }
				if (!xsnap && NEAR(o->x+o->sw, vague, x+w)) { w = o->x+o->sw-x-bw; xsnap = 1; }
				if (!ysnap && NEAR(o->y+o->sh, vague, y+h)) { h = o->y+o->sh-y-bw; ysnap = 1; }
				if (xsnap && ysnap) break;
			}
		}
		// process size hints
		if (c->xsize.flags & PMinSize)
		{
			w = MAX(w, c->xsize.min_width);
			h = MAX(h, c->xsize.min_height);
		}
		if (c->xsize.flags & PMaxSize)
		{
			w = MIN(w, c->xsize.max_width);
			h = MIN(h, c->xsize.max_height);
		}
		if (c->xsize.flags & PAspect)
		{
			double ratio = (double) w / h;
			double minr  = (double) c->xsize.min_aspect.x / c->xsize.min_aspect.y;
			double maxr  = (double) c->xsize.max_aspect.x / c->xsize.max_aspect.y;
				if (ratio < minr) h = (int)(w / minr);
			else if (ratio > maxr) w = (int)(h * maxr);
		}
		w = MAX(MINWINDOW, w); h = MAX(MINWINDOW, h);
		XMoveResizeWindow(display, ev->xmotion.window, x, y, w, h);
		// update move/req cache. allows client_flash() and handle_configurerequest to
		// play nice with mouse-based stuff
		if (c->cache)
		{
			c->cache->have_mr = 1;
			c->cache->mr_x = x; c->cache->mr_y = y;
			c->cache->mr_w = w; c->cache->mr_h = h;
			c->cache->mr_time = timestamp();
		}
	}
}

// we dont really care until a window configures and maps, so just watch it
void handle_createnotify(XEvent *ev)
{
	if (winlist_find(windows, ev->xcreatewindow.window) < 0)
	{
		wincache *cache = allocate_clear(sizeof(wincache));
		winlist_append(windows, ev->xcreatewindow.window, cache);
	}
}

// we don't track window state internally much, so this is just for info
void handle_destroynotify(XEvent *ev)
{
	Window w = ev->xdestroywindow.window;
	// remove any cached data on a window
	winlist_forget(windows, w);
	winlist_forget(windows_activated, w);
}

// very loose with configure requests
// just let stuff go through mostly unchanged so apps can remember window positions/sizes
void handle_configurerequest(XEvent *ev)
{
	client *c = client_create(ev->xconfigurerequest.window);
	if (c)
	{
		window_select(c->window);
		event_log("ConfigureRequest", c->window);
		event_client_dump(c);
		XConfigureRequestEvent *e = &ev->xconfigurerequest;
		unsigned long mask = e->value_mask & (CWX|CWY|CWWidth|CWHeight|CWBorderWidth);

		// only move/resize requests go through. never stacking
		if (e->value_mask & (CWX|CWY|CWWidth|CWHeight))
		{
			XWindowChanges wc;
			client_extended_data(c);

			wc.x = e->value_mask & CWX ? e->x: c->x;
			wc.y = e->value_mask & CWY ? e->y: c->y;
			wc.width  = e->value_mask & CWWidth  ? e->width : c->w;
			wc.height = e->value_mask & CWHeight ? e->height: c->h;
			wc.border_width = c->manage ? config_border_width: 0;
			wc.sibling = None; wc.stack_mode = None;

			// if we recently (0.1s) instructed the window to an x/y/w/h which conforms to
			// their w/h hints, demand co-operation!
			if (c->cache && c->cache->have_mr && timestamp() < c->cache->mr_time + 0.1)
			{
				mask = CWX|CWY|CWWidth|CWHeight|CWBorderWidth;
				wc.x = c->cache->mr_x; wc.y = c->cache->mr_y;
				wc.width  = c->cache->mr_w; wc.height = c->cache->mr_h;
				c->cache->have_mr = 0;
			}
			XConfigureWindow(display, c->window, mask, &wc);
		}
	}
}

// once a window has been configured, apply a border unless it is fullscreen
void handle_configurenotify(XEvent *ev)
{
	client *c;
	// we use StructureNotifyMask on root windows. this seems to be a way of detecting XRandR
	// shenanigans without actually needing to include xrandr or check for it, etc...
	// TODO: is this a legit assumption?
	if (window_is_root(ev->xconfigure.window))
	{
		Window root = ev->xconfigure.window;
		event_log("ConfigureNotify", root);
		event_note("root window change!");
		memset(cache_monitor, 0, sizeof(cache_monitor));
		ewmh_desktop_list(root);
		XWindowAttributes *attr = window_get_attributes(root);
		int i; Window w;
		// find all windows and ensure they're visible in the new screen layout
		managed_ascend(ev->xconfigure.window, i, w, c)
		{
			client_extended_data(c);
			// client_moveresize() will handle fine tuning bumping the window on-screen
			// all we have to do is get x/y in the right ballpark
			client_moveresize(c, 0,
				MIN(attr->x+attr->width-1,  MAX(attr->x, c->x)),
				MIN(attr->y+attr->height-1, MAX(attr->y, c->y)),
				c->sw, c->sh);
		}
	}
	else
	if ((c = client_create(ev->xconfigure.window)) && c->manage)
	{
		event_log("ConfigureNotify", c->window);
		event_client_dump(c);
		client_review_border(c);
		client_review_position(c);
		if (c->active && config_warp_mode == WARPFOCUS && !mouse_dragging)
		{
			client_warp_pointer(c);
			// dump any enterynotify events that have been generated
			// since this client was configured, else whe get focus jitter
			while(XCheckTypedEvent(display, EnterNotify, ev));
		}
	}
}

// map requests are when we get nasty about co-ords and size
void handle_maprequest(XEvent *ev)
{
	client *c = client_create(ev->xmaprequest.window);
	if (c && c->manage && c->initial_state == NormalState)
	{
		window_select(c->window);
		event_log("MapRequest", c->window);
		event_client_dump(c);
		client_extended_data(c);
		// if this MapRequest was already dispatched before a previous ConfigureRequest was
		// received, some clients seem to be able to map before applying the border change,
		// resulting in a little jump on screen. ensure border is done first
		client_review_border(c);
		// adjust for borders on remembered co-ords
		if (c->type == netatoms[_NET_WM_WINDOW_TYPE_NORMAL])
			{ c->w += config_border_width*2; c->h += config_border_width*2; }

		// process EWMH rules
		// above below are mutally exclusize
			if (client_rule(c, RULE_ABOVE)) client_add_state(c, netatoms[_NET_WM_STATE_ABOVE]);
		else if (client_rule(c, RULE_BELOW)) client_add_state(c, netatoms[_NET_WM_STATE_BELOW]);

		// sticky can be on anything
		if (client_rule(c, RULE_STICKY)) client_add_state(c, netatoms[_NET_WM_STATE_STICKY]);

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
		else
		if (client_rule(c, RULE_HLOCK|RULE_VLOCK))
		{
			if (client_rule(c, RULE_HLOCK)) c->cache->hlock = 1;
			if (client_rule(c, RULE_VLOCK)) c->cache->vlock = 1;
		}

		workarea active; memset(&active, 0, sizeof(workarea));

		// if a size rule exists, apply it
		if (client_rule(c, RULE_SMALL|RULE_MEDIUM|RULE_LARGE|RULE_COVER))
		{
			if (!active.w) monitor_active(c->xattr.screen, &active);
			if (client_rule(c, RULE_SMALL))  { c->sw = active.w/3; c->sh = active.h/3; }
			if (client_rule(c, RULE_MEDIUM)) { c->sw = active.w/2; c->sh = active.h/2; }
			if (client_rule(c, RULE_LARGE))  { c->sw = (active.w/3)*2; c->sh = (active.h/3)*2; }
			if (client_rule(c, RULE_COVER))  { c->sw = active.w; c->sh = active.h; }
		}

		//  if a placement rule exists, it trumps everything
		if (client_rule(c, RULE_TOP|RULE_LEFT|RULE_RIGHT|RULE_BOTTOM|RULE_SMALL|RULE_MEDIUM|RULE_LARGE|RULE_COVER))
		{
			if (!active.w) monitor_active(c->xattr.screen, &active);
			c->x = MAX(active.x, active.x + ((active.w - c->sw) / 2));
			c->y = MAX(active.y, active.y + ((active.h - c->sh) / 2));
			if (client_rule(c, RULE_BOTTOM)) c->y = active.y + active.h - c->sh;
			if (client_rule(c, RULE_RIGHT))  c->x = active.x + active.w - c->sw;
			if (client_rule(c, RULE_TOP))    c->y = active.y;
			if (client_rule(c, RULE_LEFT))   c->x = active.x;
			client_moveresize(c, 0, c->x, c->y, c->sw, c->sh);
		}
		else
		// PLACEPOINTER: center window on pointer
		if (config_window_placement == PLACEPOINTER && !(c->xsize.flags & (PPosition|USPosition)))
		{
			// figure out which monitor holds the pointer, so we can nicely keep the window on-screen
			int x, y; pointer_get(c->xattr.root, &x, &y);
			workarea a; monitor_dimensions_struts(c->xattr.screen, x, y, &a);
			client_moveresize(c, 0, MAX(a.x, x-(c->w/2)), MAX(a.y, y-(c->h/2)), c->w, c->h);
		}
		else
		// PLACEANY: windows which specify position hints are honored, all else gets centered on screen or their parent
		// PLACECENTER: centering is enforced
		if ((config_window_placement == PLACEANY && !(c->xsize.flags & (PPosition|USPosition))) || (config_window_placement == PLACECENTER))
		{
			client *p = NULL;
			workarea *m = &c->monitor;
			// try to center transients on their main window
			if (c->trans != None && (p = client_create(c->trans)) && p)
			{
				client_extended_data(p);
				m = &p->monitor;
			} else
			// center everything else on current monitor
			{
				if (!active.w) monitor_active(c->xattr.screen, &active);
				m = &active;
			}
			client_moveresize(c, 0, MAX(m->x, m->x + ((m->w - c->w) / 2)),
				MAX(m->y, m->y + ((m->h - c->h) / 2)), c->w, c->h);
		}
		// apply and rule tags
		if (client_rule(c, (TAG1|TAG2|TAG3|TAG4|TAG5|TAG6|TAG7|TAG8|TAG9)))
			c->cache->tags = c->rule->flags & (TAG1|TAG2|TAG3|TAG4|TAG5|TAG6|TAG7|TAG8|TAG9);

		// default to current tag
		if (!c->cache->tags) c->cache->tags = current_tag;

		if (c->trans == None) client_lower(c, 0);
		XSync(display, False);
	}
	XMapWindow(display, ev->xmaprequest.window);
}

// a newly mapped top-level window automatically takes focus
// this could be configurable?
void handle_mapnotify(XEvent *ev)
{
	client *c = client_create(ev->xmap.window), *a;
	if (c && c->manage && c->visible && c->initial_state == NormalState)
	{
		event_log("MapNotify", c->window);
		client_state(c, NormalState);
		// autoactivate only on:
		if ((c->cache->tags & current_tag && config_map_mode == MAPSTEAL && !client_rule(c, RULE_BLOCK)) || client_rule(c, RULE_STEAL))
		{
			// initial raise does not check -raisemode
			client_activate(c, RAISE, WARPDEF);
		} else
		{
			// if on current tag, place new window under active window and next in activate-order
			if (c->cache->tags & current_tag && (a = client_active(c->xattr.root, current_tag)) && a->window != c->window)
			{
				client_raise_under(c, a);
				winlist_forget(windows_activated, a->window);
				winlist_append(windows_activated, c->window, NULL);
				winlist_append(windows_activated, a->window, NULL);
			} else
			{
				// TODO: make this smart enough to place window on top on another tag
				winlist_forget(windows_activated, c->window);
				winlist_prepend(windows_activated, c->window, NULL);
			}
			client_flash(c, config_flash_on, config_flash_ms);
		}
		// post-placement rules. yes, can do both contract and expand in one rule. it makes sense...
		unsigned int tag = current_tag; current_tag = desktop_to_tag(tag_to_desktop(c->cache->tags));
		if (client_rule(c, RULE_CONTRACT)) client_contract(c, HORIZONTAL|VERTICAL);
		if (client_rule(c, RULE_EXPAND)) client_expand(c, HORIZONTAL|VERTICAL, 0, 0, 0, 0, 0, 0, 0, 0);
		current_tag = tag;

		ewmh_client_list(c->xattr.root);
		// some gtk windows see to need an extra kick to make them respect expose events...
		// something to do with the configurerequest step? this little nudge makes it all work :-|
		XSetWindowBorderWidth(display, c->window, 0);
		XSetWindowBorderWidth(display, c->window, config_border_width);
	}
}

// unmapping could indicate the focus window has closed
// find a new one to focus if needed
void handle_unmapnotify(XEvent *ev)
{
	int was_active = window_is_active(ev->xunmap.window);
	client *c = client_create(ev->xunmap.window);
	// was it a top-level app window that closed?
	if (c && c->manage)
	{
		event_log("UnmapNotify", c->window);
		event_client_dump(c);
		client_state(c, WithdrawnState);
	}
	// if window has already been destroyed, above client_create() may have failed
	// see if this was the active window, and if so, find someone else to take the job
	if (was_active)
	{
		if (window_is_root(ev->xunmap.event))
		{
			client_active(ev->xunmap.event, current_tag);
			ewmh_client_list(ev->xunmap.event);
		}
		else
		if ((c = client_create(ev->xunmap.event)) && c && c->manage)
		{
			client_activate(c, RAISEDEF, WARPDEF);
			ewmh_client_list(c->xattr.root);
		}
	}
}

// ClientMessage
void handle_clientmessage(XEvent *ev)
{
	event_log("ClientMessage", ev->xclient.window);
	XClientMessageEvent *m = &ev->xclient;

	if (m->message_type == netatoms[_NET_CURRENT_DESKTOP])
		tag_raise(desktop_to_tag(MAX(0, MIN(TAGS, m->data.l[0]))));
	else
	{
		client *c = client_create(m->window);
		if (c && c->manage && c->visible)
		{
			event_client_dump(c);
			if (m->message_type == netatoms[_NET_ACTIVE_WINDOW])
				client_activate(c, RAISE, WARPDEF);
			else
			if (m->message_type == netatoms[_NET_CLOSE_WINDOW])
				client_close(c);
			else
			if (m->message_type == netatoms[_NET_MOVERESIZE_WINDOW] &&
				(m->data.l[1] >= 0 || m->data.l[2] >= 0 || m->data.l[3] > 0 || m->data.l[4] > 0))
			{
				client_commit(c);
				client_extended_data(c); client_moveresize(c, 0,
					m->data.l[1] >= 0 ? m->data.l[1]: c->x,  m->data.l[2] >= 0 ? m->data.l[2]: c->y,
					m->data.l[3] >= 1 ? m->data.l[3]: c->sw, m->data.l[4] >= 1 ? m->data.l[4]: c->sh);
			}
			else
			if (m->message_type == netatoms[_NET_WM_STATE])
			{
				int i; for (i = 1; i < 2; i++)
				{
					if (m->data.l[i] == netatoms[_NET_WM_STATE_FULLSCREEN])
						client_nws_fullscreen(c, m->data.l[0]);
					else
					if (m->data.l[i] == netatoms[_NET_WM_STATE_ABOVE])
						client_nws_above(c, m->data.l[0]);
					else
					if (m->data.l[i] == netatoms[_NET_WM_STATE_BELOW])
						client_nws_below(c, m->data.l[0]);
				}
			}
		}
	}
}

// PropertyNotify
void handle_propertynotify(XEvent *ev)
{
//	while (XCheckTypedWindowEvent(display, ev->xproperty.window, PropertyNotify, ev));
	XPropertyEvent *p = &ev->xproperty;
	client *c = client_create(p->window);
	if (c && c->visible && c->manage)
	{
		if (p->atom == netatoms[_NET_WM_STATE_DEMANDS_ATTENTION] && !c->active)
			client_deactivate(c);
	}
	// clear monitor workarea/strut cache
	if (p->atom == netatoms[_NET_WM_STRUT] || p->atom == netatoms[_NET_WM_STRUT_PARTIAL])
		memset(cache_monitor, 0, sizeof(cache_monitor));
}

// sloppy focus
void handle_enternotify(XEvent *ev)
{
	// only care about the sloppy modes here
	if (config_focus_mode == FOCUSCLICK) return;
	// ensure it's a proper enter event without keys or buttons down
	if (ev->xcrossing.type != EnterNotify) return;
	// if we're in the process of dragging something, bail out
	if (mouse_dragging) return;
	// prevent focus flicker if mouse is moving through multiple windows fast
	while(XCheckTypedEvent(display, EnterNotify, ev));

	client *c = client_create(ev->xcrossing.window);
	// FOCUSSLOPPY = any manageable window
	// FOCUSSLOPPYTAG = any manageable window in current tag
	if (c && c->visible && c->manage && !c->active && (config_focus_mode == FOCUSSLOPPY ||
		(config_focus_mode == FOCUSSLOPPYTAG && c->cache->tags & current_tag)))
	{
		event_log("EnterNotify", c->window);
		event_client_dump(c);
		client_activate(c, RAISEDEF, WARPDEF);
	}
}
void handle_mappingnotify(XEvent *ev)
{
	event_log("MappingNotify", ev->xany.window);
	while(XCheckTypedEvent(display, MappingNotify, ev));
	grab_keys_and_buttons();
}
