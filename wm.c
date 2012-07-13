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

// X error handler
int oops(Display *d, XErrorEvent *ee)
{
	if (ee->error_code == BadWindow
		|| (ee->request_code == X_SetInputFocus && ee->error_code == BadMatch)
		|| (ee->request_code == X_ConfigureWindow && ee->error_code == BadMatch)
		|| (ee->request_code == X_GrabButton && ee->error_code == BadAccess)
		|| (ee->request_code == X_GrabKey && ee->error_code == BadAccess)
		) return 0;
	fprintf(stderr, "error: request code=%d, error code=%d\n", ee->request_code, ee->error_code);
	return xerror(display, ee);
}

// say something informative
void say(Screen *screen, char *txt)
{
	workarea mon; monitor_active(screen, &mon);
	int scr = XScreenNumberOfScreen(screen);
	Window root = RootWindow(display, scr);

	if (fork()) return;

	display = XOpenDisplay(0x0);

	GC gc; XftFont *font; XftDraw *draw; XftColor fg, bg; XGlyphInfo extents;

	XftColorAllocName(display, DefaultVisual(display, scr), DefaultColormap(display, scr), config_title_fg, &fg);
	XftColorAllocName(display, DefaultVisual(display, scr), DefaultColormap(display, scr), config_title_bg, &bg);

	font = XftFontOpenName(display, scr, config_title_font);
	XftTextExtentsUtf8(display, font, (unsigned char*)txt, strlen(txt), &extents);

	int line_height = font->ascent + font->descent, line_width = extents.width;
	int bar_width = MIN(line_width+20, (mon.w/10)*9), bar_height = line_height+10;

	Window bar = XCreateSimpleWindow(display, root, mon.x + mon.w - bar_width - 10, mon.y + mon.h - bar_height - 10,
		bar_width, bar_height, 1, config_flash_on, None);

	gc   = XCreateGC(display, bar, 0, 0);
	draw = XftDrawCreate(display, bar, DefaultVisual(display, scr), DefaultColormap(display, scr));

	XSetWindowAttributes attr; attr.override_redirect = True;
	XChangeWindowAttributes(display, bar, CWOverrideRedirect, &attr);
	XMapRaised(display, bar);
	XftDrawRect(draw, &bg, 0, 0, mon.w, line_height+10);
	XftDrawStringUtf8(draw, &fg, font, 10, 5 + line_height - font->descent, (unsigned char*)txt, strlen(txt));

	XSync(display, False); usleep(SAYMS*1000);
	XDestroyWindow(display, bar);

	XftDrawDestroy(draw);
	XFreeGC(display, gc);
	XftFontClose(display, font);

	exit(EXIT_SUCCESS);
}

// an X screen. may have multiple monitors, xinerama, etc
void setup_screen(int scr)
{
	int i; Window w, root = RootWindow(display, scr);
	Window supporting = XCreateSimpleWindow(display, root, 0, 0, 1, 1, 0, 0, 0);
	unsigned long pid = getpid();

	// EWMH
	XChangeProperty(display, root, netatoms[_NET_SUPPORTED], XA_ATOM, 32, PropModeReplace, (unsigned char*)netatoms, NETATOMS);

	// ewmh supporting wm
	XChangeProperty(display, root,       netatoms[_NET_SUPPORTING_WM_CHECK], XA_WINDOW, 32, PropModeReplace, (unsigned char*)&supporting, 1);
	XChangeProperty(display, supporting, netatoms[_NET_SUPPORTING_WM_CHECK], XA_WINDOW, 32, PropModeReplace, (unsigned char*)&supporting, 1);
	XChangeProperty(display, supporting, netatoms[_NET_WM_NAME], XA_STRING,    8, PropModeReplace, (const unsigned char*)"GoomwWM", 6);
	XChangeProperty(display, supporting, netatoms[_NET_WM_PID],  XA_CARDINAL, 32, PropModeReplace, (unsigned char*)&pid, 1);

	// become the window manager here
	XSelectInput(display, root, StructureNotifyMask | SubstructureRedirectMask | SubstructureNotifyMask);

	// setup any existing windows
	winlist_ascend(windows_in_play(root), i, w)
	{
		wincache *cache = allocate_clear(sizeof(wincache));
		winlist_append(windows, w, cache);
		XSelectInput(display, w, EnterWindowMask | LeaveWindowMask | FocusChangeMask | PropertyChangeMask);
		client *c = client_create(w);
		if (c && c->manage)
		{
			winlist_append(windows_activated, c->window, NULL);
			client_full_review(c);
		}
	}
	// activate and focus top window
	client_active(root, 0);
	ewmh_client_list(root);
	ewmh_desktop_list(root);
}

// window manager
int wm_main(int argc, char *argv[])
{
	int i, j, scr; XEvent ev;

	// prepare to fall back on ~/.goomwwmrc
	char *conf_home = NULL, *home = getenv("HOME");
	if (home)
	{
		conf_home = allocate_clear(1024);
		sprintf(conf_home, "%s/%s", home, CONFIGFILE);
	}

	// prepare args and merge conf file args
	int ac = argc; char **av = argv, *conf;
	if ((conf = find_arg_str(argc, argv, "-config", conf_home)))
	{
		// new list for both sets of args
		av = allocate(sizeof(char*) * ac);
		for (i = 0; i < ac; i++) av[i] = argv[i];
		// parse config line by line
		FILE *f = fopen(conf, "r");
		if (!f)
			fprintf(stderr, "could not open %s\n", conf);
		else
		{
			char *line = allocate_clear(1024);
			// yes, +1. see hyphen prepend below
			while (fgets(line+1, 1023, f))
			{
				strtrim(line+1);
				// comment or empty line
				if (!line[1] || line[1] == '#') continue;
				// nope, got a config var!
				av = reallocate(av, sizeof(char*)*(ac+2));
				char *p = line; *p++ = '-';
				// find end of arg name
				while (*p && !isspace(*p)) p++;
				*p++ = 0; av[ac++] = strdup(line);
				// find arg value, if it exists
				strtrim(p); if (*p) av[ac++] = strdup(p);
			}
			fclose(f);
			free(line);
		}
	}
	free(conf_home);

	// caches to reduce X server round trips during a single event
	cache_client = winlist_new();
	cache_xattr  = winlist_new();
	cache_inplay = winlist_new();
	memset(cache_monitor, 0, sizeof(cache_monitor));

	// do this before setting error handler, so it fails if other wm in place
	XSelectInput(display, DefaultRootWindow(display), SubstructureRedirectMask);
	XSync(display, False); xerror = XSetErrorHandler(oops); XSync(display, False);

	// determine modkey
	config_modkey = MODKEY;
	char *modkeys = find_arg_str(ac, av, "-modkey", NULL);
	if (modkeys)
	{
		config_modkey = 0;
		if (strcasestr(modkeys, "control")) config_modkey |= ControlMask;
		if (strcasestr(modkeys, "mod1"))    config_modkey |= Mod1Mask;
		if (strcasestr(modkeys, "mod2"))    config_modkey |= Mod2Mask;
		if (strcasestr(modkeys, "mod3"))    config_modkey |= Mod3Mask;
		if (strcasestr(modkeys, "mod4"))    config_modkey |= Mod4Mask;
		if (strcasestr(modkeys, "mod5"))    config_modkey |= Mod5Mask;
		if (!config_modkey) config_modkey = MODKEY;
	}
	// use by mouse-handling code
	config_ignore_modkeys = (ShiftMask|ControlMask|Mod1Mask|Mod2Mask|Mod3Mask|Mod4Mask|Mod5Mask|LockMask|NumlockMask) & ~config_modkey;

	// determine numlock mask so we can bind on keys with and without it
	XModifierKeymap *modmap = XGetModifierMapping(display);
	for (i = 0; i < 8; i++)
		for (j = 0; j < (int)modmap->max_keypermod; j++)
			if (modmap->modifiermap[i*modmap->max_keypermod+j] == XKeysymToKeycode(display, XK_Num_Lock))
				NumlockMask = (1<<i);
	// determine keysyms that trigger our modkey (used by popup menu to detect mod key release)
	memset(config_modkeycodes, 0, sizeof(config_modkeycodes)); i = 0;
	if (config_modkey & ControlMask)
		for (j = 0; i < MAXMODCODES && j < (int)modmap->max_keypermod; j++)
			config_modkeycodes[i++] = modmap->modifiermap[2*modmap->max_keypermod+j];
	if (config_modkey & Mod1Mask)
		for (j = 0; i < MAXMODCODES && j < (int)modmap->max_keypermod; j++)
			config_modkeycodes[i++] = modmap->modifiermap[3*modmap->max_keypermod+j];
	if (config_modkey & Mod2Mask)
		for (j = 0; i < MAXMODCODES && j < (int)modmap->max_keypermod; j++)
			config_modkeycodes[i++] = modmap->modifiermap[4*modmap->max_keypermod+j];
	if (config_modkey & Mod3Mask)
		for (j = 0; i < MAXMODCODES && j < (int)modmap->max_keypermod; j++)
			config_modkeycodes[i++] = modmap->modifiermap[5*modmap->max_keypermod+j];
	if (config_modkey & Mod4Mask)
		for (j = 0; i < MAXMODCODES && j < (int)modmap->max_keypermod; j++)
			config_modkeycodes[i++] = modmap->modifiermap[6*modmap->max_keypermod+j];
	if (config_modkey & Mod5Mask)
		for (j = 0; i < MAXMODCODES && j < (int)modmap->max_keypermod; j++)
			config_modkeycodes[i++] = modmap->modifiermap[7*modmap->max_keypermod+j];
	XFreeModifiermap(modmap);

	// custom keys
	for (i = 0; keyargs[i]; i++)
	{
		char *key = find_arg_str(ac, av, keyargs[i], NULL);
		if (!key) continue;

		KeySym sym = XStringToKeysym(key);
		// remove existing refs to this key, so only one action is bound
		for (j = 0; keymap[j]; j++) if (keymap[j] == sym) keymap[j] = XK_VoidSymbol;
		keymap[i] = sym;
	}

	// check for prefix key mode
	config_prefix_mode = keymap[KEY_PREFIX] == XK_VoidSymbol ? NOPREFIX: PREFIX;
	prefix_cursor = XCreateFontCursor(display, XC_icon);

	// border colors
	config_border_focus     = color_get(display, find_arg_str(ac, av, "-focus", FOCUS));
	config_border_blur      = color_get(display, find_arg_str(ac, av, "-blur",  BLUR));
	config_border_attention = color_get(display, find_arg_str(ac, av, "-attention", ATTENTION));

	// border width in pixels
	config_border_width = MAX(0, find_arg_int(ac, av, "-border", BORDER));

	// window flashing
	config_flash_on  = color_get(display, find_arg_str(ac, av, "-flashon",  FLASHON));
	config_flash_off = color_get(display, find_arg_str(ac, av, "-flashoff", FLASHOFF));
	config_flash_width = MAX(config_border_width, find_arg_int(ac, av, "-flashpx", FLASHPX));
	config_flash_ms    = MAX(FLASHMS, find_arg_int(ac, av, "-flashms", FLASHMS));

	// customizable keys
	config_switcher = find_arg_str(ac, av, "-switcher", SWITCHER);
	config_launcher = find_arg_str(ac, av, "-launcher", LAUNCHER);

	// window switcher
	config_menu_width = find_arg_int(ac, av, "-menuwidth", MENUWIDTH);
	config_menu_lines = find_arg_int(ac, av, "-menulines", MENULINES);
	config_menu_font  = find_arg_str(ac, av, "-menufont",  MENUXFTFONT);
	config_menu_fg    = find_arg_str(ac, av, "-menufg",    MENUFG);
	config_menu_bg    = find_arg_str(ac, av, "-menubg",    MENUBG);
	config_menu_bgalt = find_arg_str(ac, av, "-menubgalt", MENUBGALT);
	config_menu_hlfg  = find_arg_str(ac, av, "-menuhlfg",  MENUHLFG);
	config_menu_hlbg  = find_arg_str(ac, av, "-menuhlbg",  MENUHLBG);

	// window titles
	config_title_font = find_arg_str(ac, av, "-titlefont", TITLEXFTFONT);
	config_title_fg   = find_arg_str(ac, av, "-titlefg",   TITLEFG);
	config_title_bg   = find_arg_str(ac, av, "-titlebg",   TITLEBG);

	char *mode;

	// focus mode
	config_focus_mode = FOCUSCLICK;
	mode = find_arg_str(ac, av, "-focusmode", "click");
	if (!strcasecmp(mode, "sloppy")) config_focus_mode = FOCUSSLOPPY;
	if (!strcasecmp(mode, "sloppytag")) config_focus_mode = FOCUSSLOPPYTAG;

	// raise mode
	config_raise_mode = RAISEFOCUS;
	mode = find_arg_str(ac, av, "-raisemode", config_focus_mode == FOCUSCLICK ? "focus": "click");
	if (!strcasecmp(mode, "click")) config_raise_mode = RAISECLICK;

	// warp mode
	config_warp_mode = WARPNEVER;
	mode = find_arg_str(ac, av, "-warpmode", config_focus_mode == FOCUSCLICK ? "never": "focus");
	if (!strcasecmp(mode, "focus")) config_warp_mode = WARPFOCUS;

	// steal mode
	config_map_mode = MAPSTEAL;
	mode = find_arg_str(ac, av, "-mapmode", "steal");
	if (!strcasecmp(mode, "block")) config_map_mode = MAPBLOCK;

	// new-window placement mode
	config_window_placement = PLACEANY;
	mode = find_arg_str(ac, av, "-placement", "any");
	if (!strcasecmp(mode, "center"))  config_window_placement = PLACECENTER;
	if (!strcasecmp(mode, "pointer")) config_window_placement = PLACEPOINTER;

	// menu select mode
	config_menu_select = MENURETURN;
	if (!config_prefix_mode)
	{
		mode = find_arg_str(ac, av, "-menuselect", "return");
		if (!strcasecmp(mode, "modkeyup")) config_menu_select = MENUMODUP;
	}

	// flash title mode
	config_flash_title = 0;
	mode = find_arg_str(ac, av, "-flashtitle", "hide");
	if (!strcasecmp(mode, "show")) config_flash_title = 1;

	// app_find_or_start() keys
	for (i = 0; config_apps_keysyms[i]; i++)
	{
		char tmp[3]; sprintf(tmp, "-%d", i);
		config_apps_patterns[i] = find_arg_str(ac, av, tmp, NULL);
	}

	// load window rules
	for (i = 0; i < ac; i++) if (!strcasecmp(av[i], "-rule") && i < ac-1) rule_parse(av[i+1]);

	// window tracking
	windows = winlist_new();
	windows_activated = winlist_new();

	// init on all screens/roots
	for (scr = 0; scr < ScreenCount(display); scr++) setup_screen(scr);
	grab_keys_and_buttons();

	// auto start stuff
	if (!fork())
	{
		display = XOpenDisplay(0);
		Window root = RootWindow(display, DefaultScreen(display));
		for (i = 0; i < ac-1; i++)
		{
			if (!strcasecmp(av[i], "-exec")) exec_cmd(av[i+1]);
			else if (!strcasecmp(av[i], "-auto"))
			{
				client *a = client_find(root, av[i+1]);
				if (!a) exec_cmd(av[i+1]);
			}
		}
		exit(EXIT_SUCCESS);
	}

	// be polite
	say(DefaultScreenOfDisplay(display), "Get out of my way, Window Manager!");

	// main event loop
	for(;;)
	{
		// caches only live for a single event
		winlist_empty(cache_xattr);
		winlist_empty(cache_client);
		winlist_empty_2d(cache_inplay);

		// block and wait for something
		XNextEvent(display, &ev);
		if (ev.type == MappingNotify) handle_mappingnotify(&ev);
		if (ev.xany.window == None) continue;

		     if (ev.type == KeyPress)         handle_keypress(&ev);
		else if (ev.type == ButtonPress)      handle_buttonpress(&ev);
		else if (ev.type == ButtonRelease)    handle_buttonrelease(&ev);
		else if (ev.type == MotionNotify)     handle_motionnotify(&ev);
		else if (ev.type == CreateNotify)     handle_createnotify(&ev);
		else if (ev.type == DestroyNotify)    handle_destroynotify(&ev);
		else if (ev.type == ConfigureRequest) handle_configurerequest(&ev);
		else if (ev.type == ConfigureNotify)  handle_configurenotify(&ev);
		else if (ev.type == MapRequest)       handle_maprequest(&ev);
		else if (ev.type == MapNotify)        handle_mapnotify(&ev);
		else if (ev.type == UnmapNotify)      handle_unmapnotify(&ev);
		else if (ev.type == ClientMessage)    handle_clientmessage(&ev);
		else if (ev.type == PropertyNotify)   handle_propertynotify(&ev);
		else if (ev.type == EnterNotify)      handle_enternotify(&ev);
#ifdef DEBUG
		else fprintf(stderr, "unhandled event %d: %x\n", ev.type, (unsigned int)ev.xany.window);
		catch_exit(0);
#endif
	}
	return EXIT_SUCCESS;
}
