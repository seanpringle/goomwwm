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

// when using the -prefix key, the prefix key modifiers can be held down, or released and
// automatically implied for the final key combo. this is really designed to work with the
// common global -modkey. custom key combinations should generally work too, but might show
// foibles...
#define ISKEY(id) (keymap[id] == key && (keymodmap[id] == state ||\
	(prefix_mode_active && (keymodmap[id] & ~keymodmap[KEY_PREFIX]) == (state & ~keymodmap[KEY_PREFIX]))\
))

// MODKEY+keys
void handle_keypress(XEvent *ev)
{
	event_log("KeyPress", ev->xany.window);
	KeySym key = XkbKeycodeToKeysym(display, ev->xkey.keycode, 0, 0);
	unsigned int state = ev->xkey.state & ~(LockMask|NumlockMask);

	int i, reset_prefix = 1, reset_quit = 1;
	while(XCheckTypedEvent(display, KeyPress, ev));

	client *c = NULL;
	reset_lazy_caches();

	// by checking !prefix, we allow a second press to cancel prefix mode
	if (ISKEY(KEY_PREFIX) && !prefix_mode_active)
	{
		// activate prefix mode
		take_keyboard(ev->xany.window);
		take_pointer(ev->xany.window, ButtonPressMask, prefix_cursor);
		prefix_mode_active = 1;
		reset_prefix = 0;
	}
	else
	if (ISKEY(KEY_SWITCH))
	{
		if (config_switcher) exec_cmd(config_switcher);
		else client_switcher(0);
	}
	else
	if (ISKEY(KEY_LAUNCH)) exec_cmd(config_launcher);

	else
	// exec goomwwm. press twice
	if (ISKEY(KEY_QUIT))
	{
		if (quit_pressed_once) exit(EXIT_SUCCESS);
		quit_pressed_once = 1;
		reset_quit = 0;
	}

	// custom MODKEY launchers
	// on the command line: goomwwm -1 "firefox"
	else if ((i = in_array_keysym(config_apps_keysyms, key)) >= 0 && state == config_modkey)
		client_find_or_start(config_apps_patterns[i]);

	else if ((i = in_array_keysym(config_tags_keysyms, key)) >= 0 && state == config_modkey)
		tag_raise(1<<i);

	// tag cycling
	else if (ISKEY(KEY_TAGNEXT))  tag_raise(current_tag & TAG9 ? TAG1: current_tag<<1);
	else if (ISKEY(KEY_TAGPREV))  tag_raise(current_tag & TAG1 ? TAG9: current_tag>>1);
	else if (ISKEY(KEY_TAGONLY))  tag_only(current_tag);
	else if (ISKEY(KEY_TAGCLOSE)) tag_close(current_tag);

	else
	// following only relevant with a focused window
	if ((c = client_active(0)) && c)
	{
		client_descriptive_data(c);
		client_extended_data(c);

		int screen_x = c->monitor.x, screen_y = c->monitor.y;
		int screen_width = c->monitor.w, screen_height = c->monitor.h;
		int vague = MAX(screen_width/100, screen_height/100);

		// window co-ords translated to 0-based on screen
		int x = c->x - screen_x;
		int y = c->y - screen_y;
		int w = c->w; int h = c->h;

		// four basic window sizes
		int width1 = screen_width/3;      int height1 = screen_height/3;
		int width2 = screen_width/2;      int height2 = screen_height/2;
		int width3 = screen_width-width1; int height3 = screen_height-height1;
		int width4 = screen_width;        int height4 = screen_height;

		// final resize/move params. flags = intelligently bump / center / shrink
		int fx = 0, fy = 0, fw = 0, fh = 0; unsigned int flags = 0;

		     if (ISKEY(KEY_CLOSE))      client_close(c);
		else if (ISKEY(KEY_CYCLE))      client_cycle(c);
		else if (ISKEY(KEY_ABOVE))      client_nws_above(c, TOGGLE);
		else if (ISKEY(KEY_BELOW))      client_nws_below(c, TOGGLE);
		else if (ISKEY(KEY_STICKY))     client_nws_sticky(c, TOGGLE);
		else if (ISKEY(KEY_FULLSCREEN)) client_nws_fullscreen(c, TOGGLE);
		else if (ISKEY(KEY_HMAX))       client_nws_maxhorz(c, TOGGLE);
		else if (ISKEY(KEY_VMAX))       client_nws_maxvert(c, TOGGLE);
		else if (ISKEY(KEY_EXPAND))     client_expand(c, HORIZONTAL|VERTICAL, 0, 0, 0, 0, 0, 0, 0, 0);
		else if (ISKEY(KEY_CONTRACT))   client_contract(c, HORIZONTAL|VERTICAL);
		else if (ISKEY(KEY_VLOCK))      client_toggle_vlock(c);
		else if (ISKEY(KEY_HLOCK))      client_toggle_hlock(c);
		else if (ISKEY(KEY_HTILE))      client_htile(c);
		else if (ISKEY(KEY_VTILE))      client_vtile(c);
		else if (ISKEY(KEY_HUNTILE))    client_huntile(c);
		else if (ISKEY(KEY_VUNTILE))    client_vuntile(c);
		else if (ISKEY(KEY_UNDO))       client_rollback(c);
		else if (ISKEY(KEY_DUPLICATE))  client_duplicate(c);
		else if (ISKEY(KEY_MINIMIZE))   client_minimize(c);
		else if (ISKEY(KEY_RULE))       client_rules_apply(c, RULESRESET);
		else if (ISKEY(KEY_RULESET))    ruleset_switcher();
		else if (ISKEY(KEY_INFO))       client_flash(c, config_border_focus, FLASHMSTITLE, FLASHTITLE);

		// directional focus change
		else if (ISKEY(KEY_FOCUSLEFT))  client_focusto(c, FOCUSLEFT);
		else if (ISKEY(KEY_FOCUSRIGHT)) client_focusto(c, FOCUSRIGHT);
		else if (ISKEY(KEY_FOCUSUP))    client_focusto(c, FOCUSUP);
		else if (ISKEY(KEY_FOCUSDOWN))  client_focusto(c, FOCUSDOWN);

		// directional position swap
		else if (ISKEY(KEY_SWAPLEFT))  client_swapto(c, SWAPLEFT);
		else if (ISKEY(KEY_SWAPRIGHT)) client_swapto(c, SWAPRIGHT);
		else if (ISKEY(KEY_SWAPUP))    client_swapto(c, SWAPUP);
		else if (ISKEY(KEY_SWAPDOWN))  client_swapto(c, SWAPDOWN);

		// place client in current tag
		else if (ISKEY(KEY_TAG))
		{
			client_toggle_tag(c, current_tag, FLASH);
			ewmh_client_list();
		}
		// place client in other tags
		else if ((i = in_array_keysym(config_tags_keysyms, key)) >= 0 && state == (config_modkey|ShiftMask))
		{
			client_toggle_tag(c, 1<<i, FLASH);
			ewmh_client_list();
		}

		else
		// cycle through windows with same tag
		if (ISKEY(KEY_TSWITCH))
			client_switcher(current_tag);
		else
		// Page Up/Down rapidly moves the active window through 4 sizes
		if (!client_has_state(c, netatoms[_NET_WM_STATE_FULLSCREEN]) && (ISKEY(KEY_GROW) || ISKEY(KEY_SHRINK)))
		{
			flags |= MR_SMART; fx = c->x; fy = c->y;

			// for windows with resize increments, be a little looser detecting their zone
			if (c->xsize.flags & PResizeInc)
			{
				vague = MAX(vague, c->xsize.width_inc);
				vague = MAX(vague, c->xsize.height_inc);
			}

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
				if (ISKEY(KEY_GROW)) fh = heights[ish+1];
				if (ISKEY(KEY_SHRINK)) fh = heights[ish-1];
			} else
			if (client_has_state(c, netatoms[_NET_WM_STATE_MAXIMIZED_VERT]))
			{
				fh = screen_height;
				if (ISKEY(KEY_GROW)) fw = widths[isw+1];
				if (ISKEY(KEY_SHRINK)) fw = widths[isw-1];
			} else
			{
				int prefer_width = w > h ? 1:0;

				int is4 = (isw4 && ish4) || (isw4 && prefer_width) || (ish4 && !prefer_width) ?1:0;
				int is3 = !is4 && ((isw3 && ish3) || (isw3 && prefer_width) || (ish3 && !prefer_width)) ?1:0;
				int is2 = !is4 && !is3 && ((isw2 && ish2) || (isw2 && prefer_width) || (ish2 && !prefer_width)) ?1:0;
				int is1 = !is4 && !is3 && !is2 && ((isw1 && ish1) || (isw1 && prefer_width) || (ish1 && !prefer_width)) ?1:0;
				int is = is4 ? 5: (is3 ? 4: (is2 ? 3: (is1 ? 2: 1)));

				if (ISKEY(KEY_GROW))   { fw = widths[is+1]; fh = heights[is+1]; }
				if (ISKEY(KEY_SHRINK)) { fw = widths[is-1]; fh = heights[is-1]; }
			}
		}
		else
		// Shift+ Page Up/Down makes the focused window larger and smaller respectively
		if (!client_has_state(c, netatoms[_NET_WM_STATE_FULLSCREEN]) && (ISKEY(KEY_INC) || ISKEY(KEY_DEC)))
		{
			flags |= MR_SMART; fx = c->x; fy = c->y; fw = c->w; fh = c->h;
			int dx = screen_width/16; int dy = screen_height/16;
			if (ISKEY(KEY_INC)) { fw += dx; fh += dy; }
			if (ISKEY(KEY_DEC)) { fw -= dx; fh -= dy; }
		}

		// border snap arrow key movement
		else if (ISKEY(KEY_SNAPLEFT)  && !c->is_maxh) client_snapto(c, SNAPLEFT);
		else if (ISKEY(KEY_SNAPRIGHT) && !c->is_maxh) client_snapto(c, SNAPRIGHT);
		else if (ISKEY(KEY_SNAPUP)    && !c->is_maxv) client_snapto(c, SNAPUP);
		else if (ISKEY(KEY_SNAPDOWN)  && !c->is_maxv) client_snapto(c, SNAPDOWN);

		else
		// 3x3 arrow key movement
		if (ISKEY(KEY_UP) || ISKEY(KEY_DOWN) || ISKEY(KEY_LEFT) || ISKEY(KEY_RIGHT))
		{
			workarea mon;
			int wx = x + w/2, wy = y + h/2;
			int cx = (screen_width  - w) / 2;
			int cy = (screen_height - h) / 2;
			int done = 0;
			// expire the toggle cache
			free(c->cache->ewmh); c->cache->ewmh = NULL;

			// monitor switching if window is on an edge
			if (ISKEY(KEY_LEFT) && c->is_left)
			{
				monitor_dimensions_struts(c->monitor.x-c->monitor.l-vague, c->y, &mon);
				if (mon.x < c->monitor.x && !INTERSECT(mon.x, mon.y, mon.w, mon.h, c->monitor.x, c->monitor.y, c->monitor.h, c->monitor.w))
					{ done = 1; fx = mon.x+mon.w-w; fy = c->y; fw = w; fh = h; }
			}
			else
			if (ISKEY(KEY_RIGHT) && c->is_right)
			{
				monitor_dimensions_struts(c->monitor.x+c->monitor.w+c->monitor.r+vague, c->y, &mon);
				if (mon.x > c->monitor.x && !INTERSECT(mon.x, mon.y, mon.w, mon.h, c->monitor.x, c->monitor.y, c->monitor.h, c->monitor.w))
					{ done = 1; fx = mon.x; fy = c->y; fw = w; fh = h; }
			}
			else
			if (ISKEY(KEY_UP) && c->is_top)
			{
				monitor_dimensions_struts(c->x, c->monitor.y-c->monitor.t-vague, &mon);
				if (mon.y < c->monitor.y && !INTERSECT(mon.x, mon.y, mon.w, mon.h, c->monitor.x, c->monitor.y, c->monitor.h, c->monitor.w))
					{ done = 1; fx = c->x; fy = mon.y+mon.h-h; fw = w; fh = h; }
			}
			else
			if (ISKEY(KEY_DOWN) && c->is_bottom)
			{
				monitor_dimensions_struts(c->x, c->monitor.y+c->monitor.h+c->monitor.b+vague, &mon);
				if (mon.y > c->monitor.y && !INTERSECT(mon.x, mon.y, mon.w, mon.h, c->monitor.x, c->monitor.y, c->monitor.h, c->monitor.w))
					{ done = 1; fx = c->x; fy = mon.y; fw = w; fh = h; }
			}

			// move within current monitor
			if (!done && !c->is_full)
			{
				// MODKEY+Arrow movement occurs on a 3x3 grid for non-fullscreen, managed windows
				if (ISKEY(KEY_LEFT) && !c->is_maxh)
					{ fx = screen_x + (wx > (screen_width/2)+vague ? cx: 0); fy = screen_y+y; fw = w; fh = h; }
				else
				if (ISKEY(KEY_RIGHT) && !c->is_maxh)
					{ fx = screen_x + (wx < (screen_width/2)-vague ? cx: screen_width - w); fy = screen_y+y; fw = w; fh = h; }
				else
				if (ISKEY(KEY_UP) && !c->is_maxv)
					{ fx = screen_x+x; fy = screen_y + (wy > (screen_height/2)+vague ? cy: 0); fw = w; fh = h; }
				else
				if (ISKEY(KEY_DOWN) && !c->is_maxv)
					{ fx = screen_x+x; fy = screen_y + (wy < (screen_height/2)-vague ? cy: screen_height - h); fw = w; fh = h; }
			}
		}

		else if (ISKEY(KEY_LARGELEFT))  client_toggle_large(c, LARGELEFT);
		else if (ISKEY(KEY_LARGERIGHT)) client_toggle_large(c, LARGERIGHT);

		// no matching key combo found
		else reset_prefix = 0;

		// final co-ords are fixed. go to it...
		if (fw > 0 && fh > 0)
		{
			client_commit(c);
			client_moveresize(c, flags, fx, fy, fw, fh);
		}
	}
	// deactivate prefix mode if necessary. only one operation at a time
	if (prefix_mode_active && reset_prefix)
	{
		release_keyboard();
		release_pointer();
		prefix_mode_active = 0;
	}

	// reset quit key if a non-quit keypress arrives
	if (reset_quit)
		quit_pressed_once = 0;
}

// we bind on all mouse buttons on the root window to implement click-to-focus
// events are compressed, checked for a window change, then replayed through to clients
void handle_buttonpress(XEvent *ev)
{
	event_log("ButtonPress", ev->xbutton.subwindow);
	// all mouse button events except the wheel come here, so we can click-to-focus
	// turn off caps and num locks bits. dont care about their states
	int state = ev->xbutton.state & ~(LockMask|NumlockMask); client *c = NULL;
	int is_mod = prefix_mode_active || state & config_modkey ? 1:0;
	reset_lazy_caches();

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
			if (prefix_mode_active) release_pointer();
			take_pointer(c->window, PointerMotionMask|ButtonReleaseMask, None);
			memcpy(&mouse_attr, &c->xattr, sizeof(c->xattr));
			memcpy(&mouse_button, &ev->xbutton, sizeof(ev->xbutton));
			mouse_dragging = 1;
		}
		else
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
	int is_mod = prefix_mode_active || state & config_modkey ? 1:0;

	if (ev->xbutton.window != None && (c = client_create(ev->xbutton.window)) && c && c->manage)
	{
		event_client_dump(c);

		int xd = ev->xbutton.x_root - mouse_button.x_root;
		int yd = ev->xbutton.y_root - mouse_button.y_root;

		// if no resize or move has occurred, allow Mod+Button3 to lower a window
		if (!xd && !yd && is_mod && ev->xbutton.button == Button3)
			client_lower(c, 0);
	}
	release_pointer();
	mouse_dragging = 0;

	// deactivate prefix mode if necessary
	if (prefix_mode_active)
	{
		release_keyboard();
		prefix_mode_active = 0;
	}
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

		// client_moveresize() expects borders included, and we want that for nice, neat edge-snapping too
		if (c->decorate)
		{
			x -= c->border_width;
			y -= c->border_width;
			w += c->border_width*2;
			h += c->border_width*2;
		}

		unsigned int flags = 0;
		// snap all edges by moving window
		if (mouse_button.button == Button1) flags |= MR_SNAP;
		// snap right and bottom edges by resizing window
		if (mouse_button.button == Button3) flags |= MR_SNAPWH;
		client_moveresize(c, flags, x, y, w, h);
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
	Window win = ev->xdestroywindow.window;
	// remove any cached data on a window
	int idx = winlist_find(windows, win);
	if (idx >= 0)
	{
		wincache *cache = windows->data[idx];

		// destroy titlebar/borders
		if (cache->frame != None)
			XDestroyWindow(display, cache->frame);

		// free undo chain
		winundo *next, *undo = cache->undo;
		while (undo) { next = undo->next; free(undo); undo = next; }

		free(cache->ewmh);
	}
	winlist_forget(windows, win);
	winlist_forget(windows_activated, win);
}

// very loose with configure requests
// just let stuff go through mostly unchanged so apps can remember window positions/sizes
void handle_configurerequest(XEvent *ev)
{
	client *c = client_create(ev->xconfigurerequest.window);
	if (c)
	{
		event_log("ConfigureRequest", c->window);
		event_client_dump(c);
		window_select(c->window);
		client_extended_data(c);
		// only move/resize requests go through. never stacking
		XConfigureRequestEvent *e = &ev->xconfigurerequest;
		if (c->manage)
		{
			// client_moveresize() assumes co-ords include any border.
			// adjust the initial size to compensate
			int x = e->value_mask & CWX ? (e->x - c->border_width): c->x;
			int y = e->value_mask & CWY ? (e->y - c->border_width): c->y;
			int w = e->value_mask & CWWidth  ? (e->width  + c->border_width*2) : c->w;
			int h = e->value_mask & CWHeight ? (e->height + c->border_width*2) : c->h;
			// managed windows need to conform to a few rules
			client_moveresize(c, 0, x, y, w, h);
			client_review_border(c);
		}
		else
		{
			int x = e->value_mask & CWX ? e->x: c->x;
			int y = e->value_mask & CWY ? e->y: c->y;
			int w = e->value_mask & CWWidth  ? e->width : c->w;
			int h = e->value_mask & CWHeight ? e->height: c->h;
			// everything else can go through as it likes
			XMoveResizeWindow(display, c->window, x, y, w, h);
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
	if (ev->xconfigure.window == root)
	{
		event_log("ConfigureNotify", root);
		event_note("root window change!");
		reset_lazy_caches();
		ewmh_desktop_list();
		XWindowAttributes *attr = window_get_attributes(root);
		int i; Window w;
		// find all windows and ensure they're visible in the new screen layout
		managed_ascend(i, w, c)
		{
			client_extended_data(c);
			// client_moveresize() will handle fine tuning bumping the window on-screen
			// all we have to do is get x/y in the right ballpark
			client_moveresize(c, 0,
				MIN(attr->x+attr->width-1,  MAX(attr->x, c->x)),
				MIN(attr->y+attr->height-1, MAX(attr->y, c->y)),
				c->w, c->h);
		}
	}
	else
	if ((c = client_create(ev->xconfigure.window)))
	{
		event_log("ConfigureNotify", c->window);
		event_client_dump(c);
		if (c->manage)
		{
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
}

// map requests are when we get nasty about co-ords and size
void handle_maprequest(XEvent *ev)
{
	client *c = client_create(ev->xmaprequest.window);
#ifdef DEBUG
	if (c)
	{
		event_log("MapRequest", c->window);
		event_client_dump(c);
	}
#endif
	if (c && c->manage)
	{
		// this reset mainly for -auto apps, which may fire before docks/panels have set
		// their struts, or (apparently?) before nvidia's xinerama stuff reports all monitors
		reset_lazy_caches();
		window_select(c->window);
		client_extended_data(c);
		monitor_active(&c->monitor);
		// if this MapRequest was already dispatched before a previous ConfigureRequest was
		// received, some clients seem to be able to map before applying the border change,
		// resulting in a little jump on screen. ensure border is done first
		client_review_border(c);
		client_deactivate(c, client_active(0));
		client_rules_ewmh(c);

		// PLACEPOINTER: center window on pointer
		if (config_window_placement == PLACEPOINTER && !(c->xsize.flags & (PPosition|USPosition)))
		{
			// figure out which monitor holds the pointer, so we can nicely keep the window on-screen
			int x, y; pointer_get(&x, &y);
			workarea a; monitor_dimensions_struts(x, y, &a);
			client_moveresize(c, 0, MAX(a.x, x-(c->w/2)), MAX(a.y, y-(c->h/2)), c->w, c->h);
		}
		else
		// PLACEANY: windows which specify position hints are honored, all else gets centered on screen or their parent
		// PLACECENTER: centering is enforced
		if ((config_window_placement == PLACEANY && !(c->xsize.flags & (PPosition|USPosition))) || (config_window_placement == PLACECENTER))
		{
			client *p = NULL;
			// try to center transients on their main window
			if (c->trans != None && (p = client_create(c->trans)) && p)
			{
				client_extended_data(p);
				client_moveresize(c, 0, p->x + (p->w/2) - (c->w/2),
					p->y + (p->h/2) - (c->h/2), c->w, c->h);
			}
			else
			// center everything else on current monitor
			{
				workarea *m = &c->monitor;
				client_moveresize(c, 0, MAX(m->x, m->x + ((m->w - c->w) / 2)),
					MAX(m->y, m->y + ((m->h - c->h) / 2)), c->w, c->h);
			}
		} else
		// let program or user specified positions go through, but require it to be neatly on-screen.
		// client_moveresize() does the necessary nudging
		if (c->xsize.flags & (PPosition|USPosition))
			client_moveresize(c, 0, c->x, c->y, c->w, c->h);

		// default to current tag
		client_rules_tags(c);
		if (!c->cache->tags) client_toggle_tag(c, current_tag, NOFLASH);

		// specifying a non-active monitor will center the window there
		// this overrides PLACEPOINTER!
		client_rules_monitor(c);

		// rules may move window again
		client_rules_moveresize(c);

		// must occur after move/resize!
		client_rules_locks(c);

		// following designed to work after h/v locks
		client_rules_moveresize_post(c);

		// auto minimizing
		if (client_rule(c, RULE_MINIMIZE))
		{
			client_minimize(c);
			return;
		}

		// map frame
		if (c->decorate) XMapWindow(display, c->cache->frame);

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
#ifdef DEBUG
	if (c)
	{
		event_log("MapNotify", c->window);
		event_client_dump(c);
	}
#endif
	if (c && c->manage && c->visible)
	{
		client_set_wm_state(c, NormalState);
		client_full_review(c);
		// dont reapply rules to windows that volantarily unmapped for
		// some reason, or were explicitly minimized
		if (c->cache->has_mapped || c->minimized || c->shaded)
		{
			if (!c->shaded)
				client_activate(c, RAISE, WARPDEF);
		}
		else
		// apply rules to new windows
		{	// initial raise does not check -raisemode
			if ((c->cache->tags & current_tag && config_map_mode == MAPSTEAL && !client_rule(c, RULE_BLOCK)) || client_rule(c, RULE_STEAL))
				client_activate(c, RAISE, WARPDEF);
			else {
				// if on current tag, place new window under active window and next in activate order by default
				// if specifically raised, raise window and leave second in activate order
				// if specifically lowered, lower window and place last in activate order
				if (c->cache->tags & current_tag && (a = client_active(current_tag)) && a->window != c->window)
				{
					winlist_forget(windows_activated, c->window);
					if (client_rule(c, RULE_LOWER))
						// client was already pre-lowered in configurerequest
						winlist_prepend(windows_activated, c->window, NULL);
					else {
						if (client_rule(c, RULE_RAISE)) client_raise(c, 0); else client_raise_under(c, a);
						winlist_forget(windows_activated, a->window);
						winlist_append(windows_activated, c->window, NULL);
						winlist_append(windows_activated, a->window, NULL);
					}
				} else {
					// TODO: make this smart enough to place window on top on another tag
					winlist_forget(windows_activated, c->window);
					winlist_append(windows_activated, c->window, NULL);
				}
				client_flash(c, config_flash_on, config_flash_ms, FLASHTITLEDEF);
			}
		}
		ewmh_client_list();
		winlist_forget(windows_minimized, c->window);
		winlist_forget(windows_shaded, c->window);
		client_remove_state(c, netatoms[_NET_WM_STATE_HIDDEN]);
		client_remove_state(c, netatoms[_NET_WM_STATE_SHADED]);
		c->cache->has_mapped = 1;
	}

	// special hack for fullscreen override_redirect windows (like SDL apps) that are stupid.
	// ensure they get raised above the focused window once. after that they're on their own.
	if (c && c->xattr.override_redirect && !c->cache->is_ours)
	{
		client_extended_data(c);
		if (c->is_full) XRaiseWindow(display, c->window);
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
		if (c->minimized || c->shaded)
		{
			client_set_wm_state(c, IconicState);
			winlist_forget(windows_activated, c->window);
		} else
		{
			client_set_wm_state(c, WithdrawnState);
			window_unset_prop(c->window, netatoms[_NET_WM_STATE]);
			window_unset_prop(c->window, netatoms[_NET_WM_DESKTOP]);
			winlist_forget(windows_activated, c->window);
			winlist_forget(windows_minimized, c->window);
			winlist_forget(windows_shaded,    c->window);
		}
		// hide border
		if (c->decorate)
			XUnmapWindow(display, c->cache->frame);
	}
	// if window has already been destroyed, above client_create() may have failed
	// see if this was the active window, and if so, find someone else to take the job
	if (was_active)
	{
		if (ev->xunmap.event == root)
		{
			if (!client_active(current_tag))
				XSetInputFocus(display, PointerRoot, RevertToPointerRoot, CurrentTime);
			ewmh_client_list();
		}
		else
		if ((c = client_create(ev->xunmap.event)) && c && c->manage)
		{
			client_activate(c, RAISEDEF, WARPDEF);
			ewmh_client_list();
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
		if (c && c->manage)
		{
			event_client_dump(c);
			event_note("atom: %x", m->message_type);
			// these may occur for either minimized or normal windows
			if (m->message_type == netatoms[_NET_ACTIVE_WINDOW])
				client_activate(c, RAISE, WARPDEF);
			else
			if (m->message_type == netatoms[_NET_CLOSE_WINDOW])
				client_close(c);
			else
			if (m->message_type == netatoms[_NET_REQUEST_FRAME_EXTENTS])
				client_review_border(c);
			else
			if (c->visible)
			{
				// these only get applied to mapped windows
				if (m->message_type == netatoms[_NET_MOVERESIZE_WINDOW] &&
					(m->data.l[1] >= 0 || m->data.l[2] >= 0 || m->data.l[3] > 0 || m->data.l[4] > 0))
				{
					client_extended_data(c);
					client_commit(c);
					// to be handled following the same rules as configurenotify
					// therefore assume request does not account for borders/frame-extents
					int x = m->data.l[1] >= 0 ? m->data.l[1]: c->x;
					int y = m->data.l[2] >= 0 ? m->data.l[2]: c->y;
					int w = (m->data.l[3] >= 1 ? m->data.l[3]: c->w) + (c->border_width * 2);
					int h = (m->data.l[4] >= 1 ? m->data.l[4]: c->h) + (c->border_width * 2);
					client_moveresize(c, 0, x, y, w, h);
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
		// goomwwm cli
		if (c && (
			m->message_type == gatoms[GOOMWWM_RESTART] ||
			m->message_type == gatoms[GOOMWWM_LOG] ||
			m->message_type == gatoms[GOOMWWM_RULESET] ||
			m->message_type == gatoms[GOOMWWM_RULE] ||
			m->message_type == gatoms[GOOMWWM_FIND_OR_START] ||
			m->message_type == gatoms[GOOMWWM_NOTICE] ||
			m->message_type == gatoms[GOOMWWM_QUIT]))
		{
			event_client_dump(c);
			char *msg = window_get_text_prop(m->window, gatoms[GOOMWWM_MESSAGE]);
			event_note("msg: %s", msg);
			if (msg && m->message_type == gatoms[GOOMWWM_RESTART])
			{
				execsh(msg);
				exit(EXIT_FAILURE);
			}
			if (msg && m->message_type == gatoms[GOOMWWM_LOG])
				fprintf(stderr, "%s\n", msg);
			if (msg && m->message_type == gatoms[GOOMWWM_RULESET])
				ruleset_execute(msg);
			if (msg && m->message_type == gatoms[GOOMWWM_RULE])
				rule_execute(msg);
			if (msg && m->message_type == gatoms[GOOMWWM_FIND_OR_START])
				client_find_or_start(msg);
			if (m->message_type == gatoms[GOOMWWM_QUIT])
				exit(EXIT_SUCCESS);
			if (msg && m->message_type == gatoms[GOOMWWM_NOTICE])
			{
				char *notice = msg;
				// delay in milliseconds is prefixed
				int delay = strtol(notice, &notice, 10) * 1000;
				notification(delay ? delay: SAYMS, strtrim(notice));
			}
			free(msg);
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
		// urgency check only on inactive clients.
		// possible TODO: for active clients flash border or similar?
		if (!c->active && c->urgent && (p->atom == XA_WM_HINTS || p->atom == netatoms[_NET_WM_STATE_DEMANDS_ATTENTION]))
			client_deactivate(c, client_active(0));
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
