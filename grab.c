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

// bind to a keycode in all lock states
void grab_keycode(Window root, KeyCode keycode)
{
	XUngrabKey(display, keycode, AnyModifier, root);
	XGrabKey(display, keycode, config_modkey, root, True, GrabModeAsync, GrabModeAsync);
	XGrabKey(display, keycode, config_modkey|LockMask, root, True, GrabModeAsync, GrabModeAsync);
	if (NumlockMask)
	{
		XGrabKey(display, keycode, config_modkey|NumlockMask, root, True, GrabModeAsync, GrabModeAsync);
		XGrabKey(display, keycode, config_modkey|NumlockMask|LockMask, root, True, GrabModeAsync, GrabModeAsync);
	}
}

// grab a MODKEY+key combo
void grab_key(Window root, KeySym key)
{
	grab_keycode(root, XKeysymToKeycode(display, key));
	int i, j, min_code, max_code, syms_per_code;
	// if xmodmap is in use to remap keycodes to keysyms, a simple XKeysymToKeycode
	// may not suffice here. so we also walk the entire map of keycodes and bind to
	// each code mapped to "key"
	XDisplayKeycodes(display, &min_code, &max_code);
	KeySym *map = XGetKeyboardMapping(display, min_code, max_code-min_code, &syms_per_code);
	for (i = 0; map && i < (max_code-min_code); i++)
		for (j = 0; j < syms_per_code; j++)
			if (key == map[i*syms_per_code+j])
				grab_keycode(root, i+min_code);
	if (map) XFree(map);
}

// run at startup and on MappingNotify
void grab_keys_and_buttons()
{
	int scr, i;
	for (scr = 0; scr < ScreenCount(display); scr++)
	{
		Window root = RootWindow(display, scr);

		XUngrabKey(display, AnyKey, AnyModifier, root);
		// only grab keys if prefix mode is disabled (default)
		if (!config_prefix_mode)
		{
			for (i = 0; keymap[i]; i++) if (keymap[i] != XK_VoidSymbol) grab_key(root, keymap[i]);
			for (i = 0; config_apps_keysyms[i]; i++) if (config_apps_patterns[i]) grab_key(root, config_apps_keysyms[i]);
			for (i = 0; config_tags_keysyms[i]; i++) grab_key(root, config_tags_keysyms[i]);
		}
		// prefix mode key switches to XGrabKeyboard
		else grab_key(root, keymap[KEY_PREFIX]);

		// grab mouse buttons for click-to-focus. these get passed through to the windows
		// not binding on button4 which is usually wheel scroll
		XUngrabButton(display, AnyButton, AnyModifier, root);
		XGrabButton(display, Button1, AnyModifier, root, True, ButtonPressMask, GrabModeSync, GrabModeSync, None, None);
		XGrabButton(display, Button2, AnyModifier, root, True, ButtonPressMask, GrabModeSync, GrabModeSync, None, None);
		XGrabButton(display, Button3, AnyModifier, root, True, ButtonPressMask, GrabModeSync, GrabModeSync, None, None);
	}
}
