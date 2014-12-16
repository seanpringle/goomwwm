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

#include <sys/stat.h>
#include <errno.h>

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

// slow human interation resets some caches. theory is: if for some reason a stale
// cache is affecting goomwwm behavior, any interaction should fix it. since
// human-generated are so rare, this doesn't really affect the cache usefulness...
void reset_lazy_caches()
{
	memset(cache_monitor, 0, sizeof(cache_monitor));
}
void reset_cache_xattr()
{
	winlist_empty(cache_xattr);
}
void reset_cache_client()
{
	int i; Window w;
	winlist_ascend(cache_client, i, w)
		client_free(cache_client->data[i]);
	cache_client->len = 0;
}
void reset_cache_inplay()
{
	winlist_empty(cache_inplay);
}

// an X screen. may have multiple monitors, xinerama, etc
void setup_screen()
{
	int i; Window w;
	supporting = XCreateSimpleWindow(display, root, 0, 0, 1, 1, 0, 0, 0);
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
	winlist *l = window_children();
	winlist_ascend(l, i, w)
	{
		wincache *cache = allocate_clear(sizeof(wincache));
		winlist_append(windows, w, cache);
		client *c = client_create(w);
		if (c && c->manage && (c->visible || client_get_wm_state(c) == IconicState))
		{
			window_select(c->window);
			winlist_append(c->visible ? windows_activated: windows_shaded, c->window, NULL);
			client_full_review(c);
		}
	}
	winlist_free(l);
	// activate and focus top window
	client_active(0);
	ewmh_client_list();
	ewmh_desktop_list();
}

// identify modifiers in a key combination string
unsigned int parse_key_mask(char *keystr, unsigned int def)
{
	unsigned int modkey = 0;
	if (keystr)
	{
		if (strcasestr(keystr, "shift"))   modkey |= ShiftMask;
		if (strcasestr(keystr, "control")) modkey |= ControlMask;
		if (strcasestr(keystr, "mod1"))    modkey |= Mod1Mask;
		if (strcasestr(keystr, "mod2"))    modkey |= Mod2Mask;
		if (strcasestr(keystr, "mod3"))    modkey |= Mod3Mask;
		if (strcasestr(keystr, "mod4"))    modkey |= Mod4Mask;
		if (strcasestr(keystr, "mod5"))    modkey |= Mod5Mask;
	}
	if (!(modkey & (ControlMask|Mod1Mask|Mod2Mask|Mod3Mask|Mod4Mask|Mod5Mask)))
		modkey |= def;
	return modkey;
}

void setup_keyboard_options(int ac, char *av[])
{
	int i, j;

	// determine modkey
	char *modkeys = find_arg_str(ac, av, "-modkey", NULL);
	config_modkey = parse_key_mask(modkeys, MODKEY);

	// determine numlock mask so we can bind on keys with and without it
	XModifierKeymap *modmap = XGetModifierMapping(display);
	for (i = 0; i < 8; i++)
		for (j = 0; j < (int)modmap->max_keypermod; j++)
			if (modmap->modifiermap[i*modmap->max_keypermod+j] == XKeysymToKeycode(display, XK_Num_Lock))
				{ NumlockMask = (1<<i); break; }
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

	// everything defaults to modkey
	for (i = 0; keyargs[i]; i++)
		keymodmap[i] |= config_modkey;

	// custom keys
	for (i = 0; keyargs[i]; i++)
	{
		char tmp[32]; strcpy(tmp, keyargs[i]);
		char *key = find_arg_str(ac, av, strtrim(tmp), NULL);
		if (!key) continue;

		unsigned int mask = parse_key_mask(key, config_modkey);
		if (strcasestr(key, "nomod")) mask = 0;
		if (strrchr(key, '-')) key = strrchr(key, '-')+1;
		if (strrchr(key, '+')) key = strrchr(key, '+')+1;
		KeySym sym = XStringToKeysym(key);
		if (sym == NoSymbol)
		{
			fprintf(stderr, "unknown key: %s\n", key);
			continue;
		}
		// remove existing refs to this key, so only one action is bound
		for (j = 0; keymap[j]; j++)
			if (keymap[j] == sym && keymodmap[j] == mask)
				keymap[j] = XK_VoidSymbol;
		keymap[i] = sym;
		keymodmap[i] = mask;
	}

	// check for prefix key mode
	config_prefix_mode = keymap[KEY_PREFIX] == XK_VoidSymbol ? NOPREFIX: PREFIX;
	prefix_cursor = XCreateFontCursor(display, XC_icon);
}

void setup_general_options(int ac, char *av[])
{
	int i;
	char *mode;

	// border colors
	config_border_focus     = find_arg_str(ac, av, "-focus", FOCUS);
	config_border_blur      = find_arg_str(ac, av, "-blur",  BLUR);
	config_border_attention = find_arg_str(ac, av, "-attention", ATTENTION);

	// border width in pixels
	config_border_width = MAX(0, find_arg_int(ac, av, "-border", BORDER));

	// window flashing
	config_flash_on  = find_arg_str(ac, av, "-flashon",  FLASHON);
	config_flash_off = find_arg_str(ac, av, "-flashoff", FLASHOFF);
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
	config_menu_bc    = find_arg_str(ac, av, "-menubc",    MENUBC);

	// popup window titles
	config_title_font = find_arg_str(ac, av, "-titlefont", TITLEXFTFONT);
	config_title_fg   = find_arg_str(ac, av, "-titlefg",   TITLEFG);
	config_title_bg   = find_arg_str(ac, av, "-titlebg",   TITLEBG);
	config_title_bc   = find_arg_str(ac, av, "-titlebc",   TITLEBC);

	// title bars
	config_titlebar_font   = find_arg_str(ac, av, "-titlebarfont",  TITLEBARXFTFONT);
	config_titlebar_focus  = find_arg_str(ac, av, "-titlebarfocus", TITLEBARFOCUS);
	config_titlebar_blur   = find_arg_str(ac, av, "-titlebarblur",  TITLEBARBLUR);

	mode = find_arg_str(ac, av, "-titlebar", TITLEBAR);
	// check for specific height in pixels. any non-numeric string means 0
	config_titlebar_height = atoi(mode);
	if (!strcasecmp(mode, "on"))
	{
		textbox *tb = textbox_create(root, TB_AUTOHEIGHT, 0, 0, 1, 1, config_titlebar_font, "white", "black", NULL, NULL);
		config_titlebar_height = tb->font->ascent + tb->font->descent + config_border_width;
		textbox_free(tb);
	}

	// flash title mode
	config_flash_title = 0;
	mode = find_arg_str(ac, av, "-flashtitle", "hide");
	if (!strcasecmp(mode, "show")) config_flash_title = 1;

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
	if (!strcasecmp(mode, "follow")) config_warp_mode = WARPFOLLOW;

	// steal mode
	config_map_mode = MAPSTEAL;
	mode = find_arg_str(ac, av, "-mapmode", "steal");
	if (!strcasecmp(mode, "block")) config_map_mode = MAPBLOCK;

	// activation mode
	config_tile_mode = TILESMART;
	mode = find_arg_str(ac, av, "-tilemode", "smart");
	if (!strcasecmp(mode, "none")) config_tile_mode = TILENONE;

	// new-window placement mode
	config_window_placement = PLACEANY;
	mode = find_arg_str(ac, av, "-placement", "any");
	if (!strcasecmp(mode, "center"))  config_window_placement = PLACECENTER;
	if (!strcasecmp(mode, "pointer")) config_window_placement = PLACEPOINTER;

	// autohide non-current tags
	config_only_auto = find_arg(ac, av, "-onlyauto") >= 0 ? 1:0;

	// resize hints mode
	config_resize_inc = SMARTRESIZEINC;
	config_resizeinc_ignore = SMARTRESIZEINC_IGNORE;
	mode = find_arg_str(ac, av, "-resizehints", "smart");
	     if (!strcasecmp(mode, "all"))  config_resize_inc = RESIZEINC;
	else if (!strcasecmp(mode, "none")) config_resize_inc = NORESIZEINC;
	else if (strcasecmp(mode, "smart")) config_resizeinc_ignore = mode;

	// menu select mode
	config_menu_select = MENURETURN;
	if (!config_prefix_mode)
	{
		mode = find_arg_str(ac, av, "-menuselect", "return");
		if (!strcasecmp(mode, "modkeyup")) config_menu_select = MENUMODUP;
	}

	// optionally swap tag and app key
	mode = find_arg_str(ac, av, "-appkeys", "numbers");
	if (!strcasecmp(mode, "functions"))
	{
		for (i = 0; i < sizeof(config_apps_keysyms)/sizeof(KeySym); i++)
		{
			KeySym k = config_apps_keysyms[i];
			config_apps_keysyms[i] = config_tags_keysyms[i];
			config_tags_keysyms[i] = k;
		}
	}

	// app_find_or_start() keys
	for (i = 0; config_apps_keysyms[i]; i++)
	{
		char tmp[3]; sprintf(tmp, "-%d", i);
		config_apps_patterns[i ? i-1: 9] = find_arg_str(ac, av, tmp, NULL);
	}
}

void setup_rule_options(int ac, char *av[])
{
	int i;
	// load window rules
	// put rules in a default ruleset
	config_rulesets = allocate_clear(sizeof(winruleset));
	config_rulesets->name = strdup("[default rules]");
	for (i = 0; i < ac; i++)
	{
		// load any other rule sets
		if (!strcasecmp(av[i], "-ruleset") && i < ac-1)
		{
			config_rulesets->rules = config_rules;
			config_rules = NULL;
			winruleset *set = allocate_clear(sizeof(winruleset));
			set->name = strdup(av[++i]);
			set->next = config_rulesets;
			config_rulesets = set;
		}
		else
		if (!strcasecmp(av[i], "-rule") && i < ac-1)
			rule_parse(av[++i]);
	}
	config_rulesets->rules = config_rules;

	// default to first rule set
	winruleset *set = config_rulesets;
	while (set->next) set = set->next;
	config_rules = set->rules;
}

// window manager
int wm_main(int argc, char *argv[])
{
	int i; XEvent ev;

	char *xdg_config_home = getenv("XDG_CONFIG_HOME");
	char *home = NULL;
	char *conf_home = NULL;

	// try to use $XDG_CONFIG_HOME/goomwwm/goomwwmrc
	if (xdg_config_home)
	{
		struct stat status;
		conf_home = allocate_clear(1024);
		sprintf(conf_home, "%s/%s", xdg_config_home, CONFIGFILE);

		if (stat(conf_home, &status) == -1 && errno == ENOENT)
		{
			free(conf_home);
			home = getenv("HOME");
		}
	}
	else
		home = getenv("HOME");

	// fall back on ~/.goomwwmrc
	if (home)
	{
		conf_home = allocate_clear(1024);
		sprintf(conf_home, "%s/%s", home, CONFIGFILE_FALLBACK);
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
#ifdef DEBUG
	for (i = 0; i < ac; i++)
		printf("arg: [%s]\n", av[i]);
#endif
	// caches to reduce X server round trips during a single event
	cache_client = winlist_new();
	cache_xattr  = winlist_new();
	cache_inplay = winlist_new();
	memset(cache_monitor, 0, sizeof(cache_monitor));

	// window tracking
	windows = winlist_new();
	windows_activated = winlist_new();
	windows_minimized = winlist_new();
	windows_shaded    = winlist_new();

	// do this before setting error handler, so it fails if other wm in place
	XSelectInput(display, DefaultRootWindow(display), SubstructureRedirectMask);
	XSync(display, False); xerror = XSetErrorHandler(oops); XSync(display, False);

	setup_keyboard_options(ac, av);
	setup_general_options(ac, av);
	setup_rule_options(ac, av);
	setup_screen();
	grab_keys_and_buttons();

	// auto start stuff
	if (!fork())
	{
		display = XOpenDisplay(0);
		for (i = 0; i < ac-1; i++)
		{
			if (!strcasecmp(av[i], "-exec")) exec_cmd(av[i+1]);
			else if (!strcasecmp(av[i], "-auto"))
			{
				client *a = client_find(av[i+1]);
				if (!a) client_start(av[i+1]);
			}
		}
		exit(EXIT_SUCCESS);
	}

	// be polite
	notice("Get out of my way, Window Manager!");
	reset_lazy_caches();

	// main event loop
	for(;;)
	{
		reset_cache_xattr();

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
		else if (ev.type == Expose)           handle_expose(&ev);
#ifdef DEBUG
		else fprintf(stderr, "unhandled event %d: %x\n", ev.type, (unsigned int)ev.xany.window);
		catch_exit(0);
#endif
	}
	return EXIT_SUCCESS;
}
