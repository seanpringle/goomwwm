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

void menu_draw(textbox *text, textbox **boxes, int max_lines, int selected, char **filtered)
{
	int i;
	textbox_draw(text);
	for (i = 0; i < max_lines; i++)
	{
		textbox_font(boxes[i], config_menu_font,
			i == selected ? config_menu_hlfg: config_menu_fg,
			i == selected ? config_menu_hlbg: config_menu_bg);
		textbox_text(boxes[i], filtered[i] ? filtered[i]: "");
		textbox_draw(boxes[i]);
	}
}

int menu(char **lines, char **input, char *prompt, int selected)
{
	int line = -1, i, j, chosen = 0;
	workarea mon; monitor_active(&mon);

	int num_lines = 0; for (; lines[num_lines]; num_lines++);
	int max_lines = MIN(config_menu_lines, num_lines);
	selected = MAX(MIN(num_lines-1, selected), 0);

	int w = config_menu_width < 101 ? (mon.w/100)*config_menu_width: config_menu_width;
	int x = mon.x + (mon.w - w)/2;

	Window box = XCreateSimpleWindow(display, root, x, 0, w, 300, 1, color_get(config_menu_bc), color_get(config_menu_bg));
	XSelectInput(display, box, ExposureMask);

	// make it an unmanaged window
	window_set_atom_prop(box, netatoms[_NET_WM_STATE], &netatoms[_NET_WM_STATE_ABOVE], 1);
	window_set_atom_prop(box, netatoms[_NET_WM_WINDOW_TYPE], &netatoms[_NET_WM_WINDOW_TYPE_DOCK], 1);

	// search text input
	textbox *text = textbox_create(box, TB_AUTOHEIGHT|TB_EDITABLE, 5, 5, w-10, 1,
		config_menu_font, config_menu_fg, config_menu_bg, "", prompt);
	textbox_show(text);

	int line_height = text->font->ascent + text->font->descent;
	int row_padding = line_height/10;
	int row_height = line_height + row_padding;

	// filtered list display
	textbox **boxes = allocate_clear(sizeof(textbox*) * max_lines);

	for (i = 0; i < max_lines; i++)
	{
		boxes[i] = textbox_create(box, TB_AUTOHEIGHT, 5, (i+1) * row_height + 5, w-10, 1,
			config_menu_font, config_menu_fg, config_menu_bg, lines[i], NULL);
		textbox_show(boxes[i]);
	}

	// filtered list
	char **filtered = allocate_clear(sizeof(char*) * max_lines);
	int *line_map = allocate_clear(sizeof(int) * max_lines);
	int filtered_lines = max_lines;

	for (i = 0; i < max_lines; i++)
	{
		filtered[i] = lines[i];
		line_map[i] = i;
	}

	// resize window vertically to suit
	int h = row_height * (max_lines+1) + 10 - row_padding;
	int y = mon.y + (mon.h - h)/2;
	XMoveResizeWindow(display, box, x, y, w, h);
	XMapRaised(display, box);

	take_keyboard(box);
	for (;;)
	{
		XEvent ev;
		XNextEvent(display, &ev);

		if (ev.type == Expose)
		{
			while (XCheckTypedEvent(display, Expose, &ev));
			menu_draw(text, boxes, max_lines, selected, filtered);
		}
		else
		if (ev.type == KeyPress)
		{
			while (XCheckTypedEvent(display, KeyPress, &ev));

			int rc = textbox_keypress(text, &ev);
			if (rc < 0)
			{
				chosen = 1;
				break;
			}
			else
			if (rc)
			{
				// input changed
				for (i = 0, j = 0; i < num_lines && j < max_lines; i++)
				{
					if (strcasestr(lines[i], text->text))
					{
						line_map[j] = i;
						filtered[j++] = lines[i];
					}
				}
				filtered_lines = j;
				selected = MAX(0, MIN(selected, j-1));
				for (; j < max_lines; j++)
					filtered[j] = NULL;
			}
			else
			{
				// unhandled key
				KeySym key = XkbKeycodeToKeysym(display, ev.xkey.keycode, 0, 0);

				if (key == XK_Escape)
					break;

				if (key == XK_Up)
					selected = selected ? MAX(0, selected-1): MAX(0, filtered_lines-1);

				if (key == XK_Down || key == XK_Tab || key == XK_grave)
					selected = selected < filtered_lines-1 ? MIN(filtered_lines-1, selected+1): 0;
			}
			menu_draw(text, boxes, max_lines, selected, filtered);
		}

		// check for modkeyup mode
		if (config_menu_select == MENUMODUP && !modkey_is_down())
		{
			chosen = 1;
			break;
		}
	}
	release_keyboard();

	if (chosen && filtered[selected])
		line = line_map[selected];

	if (line < 0 && input)
		*input = strdup(text->text);

	textbox_free(text);
	for (i = 0; i < max_lines; i++)
		textbox_free(boxes[i]);
	XDestroyWindow(display, box);
	free(filtered);
	free(line_map);

	return line;
}

// simple little text input prompt based on menu
// really needs a title, or perhaps optional "prompt text>"
char* prompt(char *ps)
{
	char *input = NULL;

	workarea mon; monitor_active(&mon);

	int w = config_menu_width < 101 ? (mon.w/100)*config_menu_width: config_menu_width;
	int x = mon.x + (mon.w - w)/2;

	Window box = XCreateSimpleWindow(display, root, x, 0, w, 300, 1, color_get(config_menu_bc), color_get(config_menu_bg));
	XSelectInput(display, box, ExposureMask);

	// make it an unmanaged window
	window_set_atom_prop(box, netatoms[_NET_WM_STATE], &netatoms[_NET_WM_STATE_ABOVE], 1);
	window_set_atom_prop(box, netatoms[_NET_WM_WINDOW_TYPE], &netatoms[_NET_WM_WINDOW_TYPE_DOCK], 1);

	// textbox
	textbox *text = textbox_create(box, TB_EDITABLE|TB_AUTOHEIGHT, 5, 5, w-10, 1, config_menu_font, config_menu_fg, config_menu_bg, "", ps);

	// textbox has computed line height for the supplied font
	// resize window vertically to suit
	int h = text->h + 10;
	int y = mon.y + (mon.h - h)/2;
	XMoveResizeWindow(display, box, x, y, w, h);

	textbox_show(text);
	XMapRaised(display, box);

	take_keyboard(box);
	for (;;)
	{
		XEvent ev;
		XNextEvent(display, &ev);

		if (ev.type == Expose)
		{
			while (XCheckTypedEvent(display, Expose, &ev));
			textbox_draw(text);
		}
		else
		if (ev.type == KeyPress)
		{
			while (XCheckTypedEvent(display, KeyPress, &ev));

			int rc = textbox_keypress(text, &ev);
			if (rc < 0)
			{
				// text entered
				input = strdup(text->text);
				break;
			}
			else
			if (rc)
			{
				// selection changed
				textbox_draw(text);
			}
			else
			{
				// unhandled key
				KeySym key = XkbKeycodeToKeysym(display, ev.xkey.keycode, 0, 0);
				if (key == XK_Escape) break;
			}
		}
	}
	release_keyboard();

	textbox_free(text);
	XDestroyWindow(display, box);

	return input;
}
