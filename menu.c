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

// redraw the popup menu window
void menu_draw(struct localmenu *my)
{
	int i, n;
	XGlyphInfo extents;

	// draw text input bar
	int bar_len       = strlen(my->input) + strlen(my->prompt);
	int cursor_offset = MIN(my->offset + strlen(my->prompt), bar_len);
	int cursor_width  = MAX(2, my->line_height/10);
	char *bar = alloca(bar_len + 10);
	sprintf(bar, "%s%s", my->prompt, my->input);

	// replace spaces so XftTextExtents8 includes their width. why does it strip trailing spaces? crazy...
	for (i = 0; i < bar_len; i++) if (isspace(bar[i])) bar[i] = '_';

	// calc cursor position
	XftTextExtents8(display, my->font, (unsigned char*)bar, cursor_offset, &extents);
	int cursor_x = extents.width;

	// calc full input text width
	XftTextExtents8(display, my->font, (unsigned char*)bar, bar_len, &extents);

	// restore correct input string
	sprintf(bar, "%s%s", my->prompt, my->input);

	XftDrawRect(my->draw, &my->bg, 0, 0, my->width, my->height);
	XftDrawString8(my->draw, &my->fg, my->font, my->horz_pad, my->vert_pad+my->line_height-my->font->descent, (unsigned char*)bar, bar_len);
	XftDrawRect(my->draw, &my->fg, cursor_x + my->horz_pad + cursor_width, my->vert_pad+2, cursor_width, my->line_height-4);

	// filter lines by current input text
	memset(my->filtered, 0, sizeof(char*) * (my->num_lines+1));
	for (i = 0, n = 0; my->lines[i]; i++)
		if (!my->offset || strcasestr(my->lines[i], my->input))
			my->filtered[n++] = my->lines[i];
	// vertical bounds of highlight bar
	my->current = MAX(0, MIN(my->current, n-1));
	for (i = 0; i < my->max_lines && my->filtered[i]; i++)
	{
		XftColor fg = my->fg;
		// vertical position of *top* of current line
		int y = my->vert_pad+(my->line_height*(i+1));
		// http://en.wikipedia.org/wiki/Typeface#Font_metrics
		int font_baseline = y + my->line_height - my->font->descent -2;
		// are we highlighting this line?
		if (i == my->current)
		{
			fg = my->hlfg;
			XftDrawRect(my->draw, &my->hlbg, my->horz_pad, y, my->width-(my->horz_pad*2), my->line_height);
		} else
		if (!(i%2))
		{
			// shade alternate lines for better readability
			XftDrawRect(my->draw, &my->bgalt, my->horz_pad, y, my->width-(my->horz_pad*2), my->line_height);
		}
		XftDrawString8(my->draw, &fg, my->font, my->horz_pad, font_baseline, (unsigned char*)my->filtered[i], strlen(my->filtered[i]));
	}
	// double buffering
	XCopyArea(display, my->canvas, my->window, my->gc, 0, 0, my->width, my->height, 0, 0);
}

// select currently highlighted line and exit
void menu_select_current(struct localmenu *my)
{
	if (my->filtered[my->current])
		my->selected = my->filtered[my->current];
	my->done = 1;
}

// handle popup menu text input for filtering
void menu_key(struct localmenu *my, XEvent *ev)
{
	char pad[32]; KeySym key; Status stat;
	int len = XmbLookupString(my->xic, &ev->xkey, pad, sizeof(pad), &key, &stat);
	if (stat == XBufferOverflow) return;
	pad[len] = 0;

	unsigned int state = ev->xkey.state;
	key = XkbKeycodeToKeysym(display, ev->xkey.keycode, 0, 0);
	int input_len = strlen(my->input);

	if (key == XK_Escape)
	{
		my->done = 1;
		my->input[0] = 0;
	}
	else
	if (key == XK_BackSpace && my->offset > 0)
	{
		// within text bounds
		if (my->offset < input_len)
		{
			int chars = input_len - my->offset;
			char *src = my->input + my->offset;
			char *dst = src-1;
			memmove(dst, src, chars);
			my->input[input_len-1] = 0;
			my->offset--;
		}
		else my->input[--(my->offset)] = 0;
	}
	else
	// only fires if within text bounds
	if (key == XK_Delete && my->offset < input_len)
	{
		int chars = input_len - my->offset;
		char *dst = my->input + my->offset;
		char *src = dst+1;
		memmove(dst, src, chars);
		my->input[input_len-1] = 0;
	}
	else
	// beginning of line
	if (key == XK_Home)
		my->offset = 0;
	else
	// end of line
	if (key == XK_End)
		my->offset = input_len;
	else
	// scan for previous word gap
	if (key == XK_Left && state & ControlMask)
	{
		for (; my->offset > 0 && isspace(my->input[my->offset-1]); my->offset--);
		for (; my->offset > 0 && !isspace(my->input[my->offset-1]); my->offset--);
	}
	else
	// scan for next word gap
	if (key == XK_Right && state & ControlMask)
	{
		for (; my->offset < input_len && !isspace(my->input[my->offset]); my->offset++);
		for (; my->offset < input_len && isspace(my->input[my->offset]); my->offset++);
	}
	else
	// cursor left one char
	if (key == XK_Left)
		my->offset = MAX(0, my->offset-1);
	else
	// cursor right one char
	if (key == XK_Right)
		my->offset = MIN(input_len, my->offset+1);
	else
	// change line selection
	if (key == XK_Up || key == XK_KP_Up || key == XK_KP_Subtract)
		my->current = (my->current == 0 ? my->max_lines-1: my->current-1);
	else
	// TODO: XK_Tab and XK_grave shouldn't be hardcoded here. should check keymap intelligently
	if (key == XK_Down || key == XK_KP_Down || key == XK_KP_Add || key == XK_Tab || key == XK_grave)
		my->current = (my->current == my->max_lines-1 ? 0: my->current+1);
	else
	if (key == XK_Return || key == XK_KP_Enter)
		menu_select_current(my);
	else
	// normal character insert
	if (!iscntrl(*pad) && my->offset < my->input_size-1)
	{
		// inserting text
		if (my->offset < input_len)
		{
			// be lazy, but safe with buffer bounds
			my->input = reallocate(my->input, input_len+2); // +2 for trailing null, and new char
			int chars = input_len - my->offset;
			char *src = my->input + my->offset;
			char *dst = src+1;
			memmove(dst, src, chars);
			my->input[my->offset++] = *pad;
			my->input[input_len+1] = 0;
		} else
		// appending text
		{
			my->input[my->offset++] = *pad;
			my->input[my->offset] = 0;
		}
	}
	menu_draw(my);
}

// menu
int menu(char **lines, char **input, char *prompt, int firstsel)
{
	int i, l;
	struct localmenu _my, *my = &_my;
	workarea mon; monitor_active(&mon);

	// this never fails, afaics. we get some sort of font, no matter what
	my->font = XftFontOpenName(display, screen_id, config_menu_font);
	XftColorAllocName(display, DefaultVisual(display, screen_id), DefaultColormap(display, screen_id), config_menu_fg,    &my->fg);
	XftColorAllocName(display, DefaultVisual(display, screen_id), DefaultColormap(display, screen_id), config_menu_bg,    &my->bg);
	XftColorAllocName(display, DefaultVisual(display, screen_id), DefaultColormap(display, screen_id), config_menu_bgalt, &my->bgalt);
	XftColorAllocName(display, DefaultVisual(display, screen_id), DefaultColormap(display, screen_id), config_menu_hlfg,  &my->hlfg);
	XftColorAllocName(display, DefaultVisual(display, screen_id), DefaultColormap(display, screen_id), config_menu_hlbg,  &my->hlbg);
	my->line_height = my->font->ascent + my->font->descent +4; // +2 pixel extra line spacing

	for (l = 0, i = 0; lines[i]; i++) l = MAX(l, strlen(lines[i]));

	my->lines       = lines;
	my->num_lines   = i;
	my->max_lines   = MIN(config_menu_lines, my->num_lines);
	my->input_size  = MAX(l, 100);
	my->filtered    = allocate_clear(sizeof(char*) * (my->num_lines+1));
	my->input       = allocate_clear(my->input_size+1);
	my->current     = firstsel; // index of currently highlighted line
	my->offset      = 0; // length of text in input buffer
	my->done        = 0; // bailout flag
	my->horz_pad    = 5; // horizontal padding
	my->vert_pad    = 5; // vertical padding
	my->width       = config_menu_width < 101 ? (mon.w/100)*config_menu_width: config_menu_width;
	my->height      = ((my->line_height) * (my->max_lines+1)) + (my->vert_pad*2);
	my->xbg         = color_get(config_menu_bg);
	my->selected    = NULL;
	my->prompt      = prompt;

	int x = mon.x + ((mon.w - my->width)/2);
	int y = mon.y + (mon.h/2) - (my->height/2);
	int b = 1;

	my->window = XCreateSimpleWindow(display, root, x-b, y-b, my->width, my->height, b, color_get(config_menu_bc), my->xbg);
	// make it an unmanaged window
	window_set_atom_prop(my->window, netatoms[_NET_WM_STATE], &netatoms[_NET_WM_STATE_ABOVE], 1);
	window_set_atom_prop(my->window, netatoms[_NET_WM_WINDOW_TYPE], &netatoms[_NET_WM_WINDOW_TYPE_DOCK], 1);
	XSelectInput(display, my->window, ExposureMask|KeyPressMask);

	// drawing environment
	my->gc     = XCreateGC(display, my->window, 0, 0);
	my->canvas = XCreatePixmap(display, root, my->width, my->height, DefaultDepth(display, screen_id));
	my->draw   = XftDrawCreate(display, my->canvas, DefaultVisual(display, screen_id), DefaultColormap(display, screen_id));

	// input keymap->charmap handling
	my->xim = XOpenIM(display, NULL, NULL, NULL);
	my->xic = XCreateIC(my->xim, XNInputStyle, XIMPreeditNothing | XIMStatusNothing, XNClientWindow, my->window, XNFocusWindow, my->window, NULL);

	menu_draw(my);
	XMapRaised(display, my->window);
	if (!take_keyboard(my->window))
	{
		fprintf(stderr, "cannot grab keyboard!\n");
		return my->max_lines;
	}
	menu_draw(my);
	// main event loop
	for(;!my->done;)
	{
		XEvent ev;
		XNextEvent(display, &ev);
		if (ev.type == Expose)
			menu_draw(my);
		else
		if (ev.type == KeyPress)
			menu_key(my, &ev);

		if (config_menu_select == MENUMODUP && !modkey_is_down())
			menu_select_current(my);
	}
	free(my->filtered);
	XftDrawDestroy(my->draw);
	XFreeGC(display, my->gc);
	XftFontClose(display, my->font);
	XDestroyWindow(display, my->window);
	release_keyboard();

	// return input to caller if no line was selected
	if (!my->selected && input) *input = my->input; else free(my->input);

	if (my->selected)
		for (i = 0; my->lines[i]; i++)
			if (my->lines[i] == my->selected)
				return i;
	return -1;
}

// simple little text input prompt based on menu
// really needs a title, or perhaps optional "prompt text>"
char* prompt(char *ps)
{
	char *line = NULL, *input = NULL;
	menu(&line, &input, ps, 0);
	return input;
}
