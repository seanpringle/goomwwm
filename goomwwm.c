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

#define _GNU_SOURCE
#include <X11/X.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xmd.h>
#include <X11/Xutil.h>
#include <X11/Xproto.h>
#include <X11/keysym.h>
#include <X11/XKBlib.h>
#include <X11/Xft/Xft.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <ctype.h>
#include <math.h>
#include <signal.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <X11/extensions/Xinerama.h>

#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define NEAR(a,o,b) ((b) > (a)-(o) && (b) < (a)+(o))
#define OVERLAP(a,b,c,d) (((a)==(c) && (b)==(d)) || MIN((a)+(b), (c)+(d)) - MAX((a), (c)) > 0)
#define INTERSECT(x,y,w,h,x1,y1,w1,h1) (OVERLAP((x),(w),(x1),(w1)) && OVERLAP((y),(h),(y1),(h1)))
#define READ 0
#define WRITE 1
#define HORIZONTAL 1
#define VERTICAL 2

#define FOCUSLEFT 1
#define FOCUSRIGHT 2
#define FOCUSUP 3
#define FOCUSDOWN 4

void* allocate(unsigned long bytes)
{
	void *ptr = malloc(bytes);
	if (!ptr)
	{
		fprintf(stderr, "malloc failed!\n");
		exit(EXIT_FAILURE);
	}
	return ptr;
}
void* allocate_clear(unsigned long bytes)
{
	void *ptr = allocate(bytes);
	memset(ptr, 0, bytes);
	return ptr;
}
void* reallocate(void *ptr, unsigned long bytes)
{
	ptr = realloc(ptr, bytes);
	if (!ptr)
	{
		fprintf(stderr, "realloc failed!\n");
		exit(EXIT_FAILURE);
	}
	return ptr;
}

void strtrim(char *str)
{
	int i = 0, j = 0;
	while (isspace(str[i])) i++;
	while (str[i]) str[i++] = str[j++];
	while (isspace(str[--j]));
	str[++j] = '\0';
}

void catch_exit(int sig)
{
	while (0 < waitpid(-1, NULL, WNOHANG));
}
// execute sub-process
pid_t exec_cmd(char *cmd)
{
	if (!cmd || !cmd[0]) return -1;
	signal(SIGCHLD, catch_exit);
	pid_t pid = fork();
	if (!pid)
	{
		setsid();
		execlp("/bin/sh", "sh", "-c", cmd, NULL);
		exit(EXIT_FAILURE);
	}
	return pid;
}

// cli arg handling
int find_arg(int argc, char *argv[], char *key)
{
	int i; for (i = 0; i < argc && strcasecmp(argv[i], key); i++);
	return i < argc ? i: -1;
}
char* find_arg_str(int argc, char *argv[], char *key, char* def)
{
	int i = find_arg(argc, argv, key);
	return (i > 0 && i < argc-1) ? argv[i+1]: def;
}
int find_arg_opts(int argc, char *argv[], char *key, char **list, int count)
{
	char *s = find_arg_str(argc, argv, key, NULL);
	int i; for (i = 0; i < count; i++) if (s && !strcasecmp(s, list[i])) return i;
	return -1;
}
int find_arg_int(int argc, char *argv[], char *key, int def)
{
	int i = find_arg(argc, argv, key);
	return (i > 0 && i < argc-1) ? strtol(argv[i+1], NULL, 10): def;
}

#define CLIENTTITLE 100
#define CLIENTCLASS 50
#define CLIENTNAME 50
#define CLIENTSTATE 10

// window lists
typedef struct {
	Window *array;
	void **data;
	int len;
} winlist;

#define winlist_ascend(l,i,w) for ((i) = 0; (i) < (l)->len && (((w) = (l)->array[i]) || 1); (i)++)
#define winlist_descend(l,i,w) for ((i) = (l)->len-1; (i) >= 0 && (((w) = (l)->array[i]) || 1); (i)--)

#define TOPLEFT 1
#define TOPRIGHT 2
#define BOTTOMLEFT 3
#define BOTTOMRIGHT 4

#define UNDO 10

typedef struct {
	int x, y, w, h;
	int sx, sy, sw, sh;
	int states;
	Atom state[CLIENTSTATE];
} winundo;

// track window stuff
typedef struct {
	int have_old;
	int x, y, w, h;
	int sx, sy, sw, sh;
	int have_mr;
	int mr_x, mr_y, mr_w, mr_h;
	int have_closed;
	int last_corner;
	unsigned int tags;
	int undo_levels;
	winundo undo[UNDO];
} wincache;

#define TAG1 1
#define TAG2 (1<<1)
#define TAG3 (1<<2)
#define TAG4 (1<<3)
#define TAG5 (1<<4)
#define TAG6 (1<<5)
#define TAG7 (1<<6)
#define TAG8 (1<<7)
#define TAG9 (1<<8)

#define TAGS 9

// usable space on a monitor
typedef struct {
	int x, y, w, h;
	int l, r, t, b;
} workarea;

// a managable window
typedef struct {
	Window window, trans;
	XWindowAttributes xattr;
	XSizeHints xsize;
	int manage, visible, input, focus, active,
		x, y, w, h, sx, sy, sw, sh,
		is_full, is_left, is_top, is_right, is_bottom,
		is_xcenter, is_ycenter, is_maxh, is_maxv, states;
	char title[CLIENTTITLE], class[CLIENTCLASS], name[CLIENTNAME];
	Atom state[CLIENTSTATE], type;
	workarea monitor;
	wincache *cache;
} client;

// just defaults, mostly configurable from command line
#define BORDER 2
#define FOCUS "Royal Blue"
#define BLUR "Dark Gray"
#define ATTENTION "Red"
#define FLASHON "Dark Green"
#define FLASHOFF "Dark Red"
#define SWITCHER NULL
#define LAUNCHER "dmenu_run"
#define FLASHPX 20
#define FLASHMS 300
#define MODKEY Mod4Mask
#define MENUXFTFONT "mono-14"
#define MENUWIDTH 50
#define MENULINES 25
#define MENUFG "#cccccc"
#define MENUBG "#222222"
#define MENUHLFG "#ffffff"
#define MENUHLBG "#005577"
#define CONFIGFILE ".goomwwmrc"

#define FOCUSCLICK 1
#define FOCUSSLOPPY 2
#define FOCUSSLOPPYTAG 3

#define RAISE 1
#define RAISEDEF 0

#define WARP 1
#define WARPDEF 0

#define RAISEFOCUS 1
#define RAISECLICK 2

#define WARPFOCUS 1
#define WARPNEVER 0

#define PLACEANY 1
#define PLACECENTER 2
#define PLACEPOINTER 3

#define FLASH 1
#define NOFLASH 0

unsigned int config_modkey, config_ignore_modkeys,
	config_border_focus, config_border_blur, config_border_attention,
	config_flash_on, config_flash_off, config_warp_mode,
	config_border_width, config_flash_width, config_flash_ms,
	config_menu_width, config_menu_lines, config_focus_mode, config_raise_mode, config_window_placement;

char *config_menu_font, *config_menu_fg, *config_menu_bg, *config_menu_hlfg, *config_menu_hlbg;

char *config_switcher, *config_launcher, *config_apps_patterns[10];
KeySym config_apps_keysyms[] = { XK_0, XK_1, XK_2, XK_3, XK_4, XK_5, XK_6, XK_7, XK_8, XK_9, 0 };
KeySym config_tags_keysyms[] = { XK_F1, XK_F2, XK_F3, XK_F4, XK_F5, XK_F6, XK_F7, XK_F8, XK_F9, 0 };

int in_array_keysym(KeySym *array, KeySym code)
{
	int i; for (i = 0; array[i]; i++)
		if (array[i] == code) return i;
	return -1;
}

#define KEY_ENUM(a,b,c) a
#define KEY_KSYM(a,b,c) [a] = b
#define KEY_CARG(a,b,c) #c

// default keybindings
#define KEYLIST(X) \
	X(KEY_RIGHT, XK_Right, -right),\
	X(KEY_LEFT, XK_Left, -left),\
	X(KEY_UP, XK_Up, -up),\
	X(KEY_DOWN, XK_Down, -down),\
	X(KEY_FOCUSRIGHT, XK_l, -focusright),\
	X(KEY_FOCUSLEFT, XK_j, -focusleft),\
	X(KEY_FOCUSUP, XK_i, -focusup),\
	X(KEY_FOCUSDOWN, XK_k, -focusdown),\
	X(KEY_SHRINK, XK_Page_Down, -shrink),\
	X(KEY_GROW, XK_Page_Up, -grow),\
	X(KEY_FULLSCREEN, XK_f, -fullscreen),\
	X(KEY_ABOVE, XK_a, -above),\
	X(KEY_BELOW, XK_b, -below),\
	X(KEY_STICKY, XK_s, -sticky),\
	X(KEY_VMAX, XK_Home, -vmax),\
	X(KEY_HMAX, XK_End, -hmax),\
	X(KEY_EXPAND, XK_Return, -expand),\
	X(KEY_CONTRACT, XK_BackSpace, -contract),\
	X(KEY_EVMAX, XK_Insert, -evmax),\
	X(KEY_EHMAX, XK_Delete, -ehmax),\
	X(KEY_TAG, XK_t, -tag),\
	X(KEY_SWITCH, XK_Tab, -switch),\
	X(KEY_TSWITCH, XK_grave, -tswitch),\
	X(KEY_CYCLE, XK_c, -cycle),\
	X(KEY_CLOSE, XK_Escape, -close),\
	X(KEY_HTILE, XK_h, -htile),\
	X(KEY_VTILE, XK_v, -vtile),\
	X(KEY_UNDO, XK_u, -undo),\
	X(KEY_TAGNEXT, XK_m, -tagnext),\
	X(KEY_TAGPREV, XK_n, -tagprev),\
	X(KEY_DEBUG, XK_d, -debug),\
	X(KEY_LAUNCH, XK_x, -launch)

enum { KEYLIST(KEY_ENUM) };
KeySym keymap[] = { KEYLIST(KEY_KSYM), 0 };
char *keyargs[] = { KEYLIST(KEY_CARG), NULL };

unsigned int NumlockMask = 0;
Display *display;
XButtonEvent mouse_button;
XWindowAttributes mouse_attr;
winlist *windows, *windows_activated;
unsigned int current_tag = TAG1;

winlist *cache_client;
winlist *cache_xattr;
winlist *cache_inplay;

static int (*xerror)(Display *, XErrorEvent *);

#define ATOM_ENUM(x) x
#define ATOM_CHAR(x) #x

#define ICCCM_ATOMS(X) \
	X(WM_DELETE_WINDOW),\
	X(WM_STATE),\
	X(WM_TAKE_FOCUS),\
	X(WM_NAME),\
	X(WM_CLASS),\
	X(WM_WINDOW_ROLE),\
	X(WM_PROTOCOLS)

enum { ICCCM_ATOMS(ATOM_ENUM), ATOMS };
const char *atom_names[] = { ICCCM_ATOMS(ATOM_CHAR) };
Atom atoms[ATOMS];

#define EWMH_ATOMS(X) \
	X(_NET_SUPPORTING_WM_CHECK),\
	X(_NET_CLIENT_LIST),\
	X(_NET_CLIENT_LIST_STACKING),\
	X(_NET_NUMBER_OF_DESKTOPS),\
	X(_NET_CURRENT_DESKTOP),\
	X(_NET_DESKTOP_GEOMETRY),\
	X(_NET_DESKTOP_VIEWPORT),\
	X(_NET_WORKAREA),\
	X(_NET_ACTIVE_WINDOW),\
	X(_NET_CLOSE_WINDOW),\
	X(_NET_MOVERESIZE_WINDOW),\
	X(_NET_WM_NAME),\
	X(_NET_WM_PID),\
	X(_NET_WM_WINDOW_TYPE),\
	X(_NET_WM_WINDOW_TYPE_DESKTOP),\
	X(_NET_WM_WINDOW_TYPE_DOCK),\
	X(_NET_WM_WINDOW_TYPE_SPLASH),\
	X(_NET_WM_WINDOW_TYPE_UTILITY),\
	X(_NET_WM_WINDOW_TYPE_TOOLBAR),\
	X(_NET_WM_WINDOW_TYPE_MENU),\
	X(_NET_WM_WINDOW_TYPE_DIALOG),\
	X(_NET_WM_WINDOW_TYPE_NORMAL),\
	X(_NET_WM_STATE),\
	X(_NET_WM_STATE_MODAL),\
	X(_NET_WM_STATE_STICKY),\
	X(_NET_WM_STATE_MAXIMIZED_VERT),\
	X(_NET_WM_STATE_MAXIMIZED_HORZ),\
	X(_NET_WM_STATE_SHADED),\
	X(_NET_WM_STATE_SKIP_TASKBAR),\
	X(_NET_WM_STATE_SKIP_PAGER),\
	X(_NET_WM_STATE_HIDDEN),\
	X(_NET_WM_STATE_FULLSCREEN),\
	X(_NET_WM_STATE_ABOVE),\
	X(_NET_WM_STATE_BELOW),\
	X(_NET_WM_STATE_DEMANDS_ATTENTION),\
	X(_NET_WM_STATE_ADD),\
	X(_NET_WM_STATE_REMOVE),\
	X(_NET_WM_STATE_TOGGLE),\
	X(_NET_WM_ALLOWED_ACTIONS),\
	X(_NET_WM_ACTION_MOVE),\
	X(_NET_WM_ACTION_RESIZE),\
	X(_NET_WM_ACTION_MINIMIZE),\
	X(_NET_WM_ACTION_SHADE),\
	X(_NET_WM_ACTION_STICK),\
	X(_NET_WM_ACTION_MAXIMIZE_VERT),\
	X(_NET_WM_ACTION_MAXIMIZE_HORZ),\
	X(_NET_WM_ACTION_FULLSCREEN),\
	X(_NET_WM_ACTION_CHANGE_DESKTOP),\
	X(_NET_WM_ACTION_CLOSE),\
	X(_NET_WM_STRUT),\
	X(_NET_WM_STRUT_PARTIAL),\
	X(_NET_WM_DESKTOP),\
	X(_NET_SUPPORTED)

enum { EWMH_ATOMS(ATOM_ENUM), NETATOMS };
const char *netatom_names[] = { EWMH_ATOMS(ATOM_CHAR) };
Atom netatoms[NETATOMS];

#define ADD 1
#define REMOVE 0
#define TOGGLE 2

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

#define WINLIST 32

winlist* winlist_new()
{
	winlist *l = allocate(sizeof(winlist)); l->len = 0;
	l->array = allocate(sizeof(Window) * (WINLIST+1));
	l->data  = allocate(sizeof(void*) * (WINLIST+1));
	return l;
}
int winlist_append(winlist *l, Window w, void *d)
{
	if (l->len > 0 && !(l->len % WINLIST))
	{
		l->array = reallocate(l->array, sizeof(Window) * (l->len+WINLIST+1));
		l->data  = reallocate(l->data,  sizeof(void*)  * (l->len+WINLIST+1));
	}
	l->data[l->len] = d;
	l->array[l->len++] = w;
	return l->len-1;
}
void winlist_empty(winlist *l)
{
	while (l->len > 0) free(l->data[--(l->len)]);
}
void winlist_free(winlist *l)
{
	winlist_empty(l); free(l->array); free(l->data); free(l);
}
void winlist_empty_2d(winlist *l)
{
	while (l->len > 0) winlist_free(l->data[--(l->len)]);
}
int winlist_find(winlist *l, Window w)
{
	// iterate backwards. theory is: windows most often accessed will be
	// nearer the end. testing with kcachegrind seems to support this...
	int i; Window o; winlist_descend(l, i, o) if (w == o) return i;
	return -1;
}
int winlist_forget(winlist *l, Window w)
{
	int i, j;
	for (i = 0, j = 0; i < l->len; i++, j++)
	{
		l->array[j] = l->array[i];
		l->data[j]  = l->data[i];
		if (l->array[i] == w) { free(l->data[i]); j--; }
	}
	l->len -= (i-j);
	return j != i ?1:0;
}

// allocate a pixel value for an X named color
unsigned int color_get(Display *d, const char *name)
{
	XColor color;
	Colormap map = DefaultColormap(d, DefaultScreen(d));
	return XAllocNamedColor(d, map, name, &color, &color) ? color.pixel: None;
}

// find mouse pointer location
int pointer_get(Window root, int *x, int *y)
{
	*x = 0; *y = 0;
	Window rr, cr; int rxr, ryr, wxr, wyr; unsigned int mr;
	if (XQueryPointer(display, root, &rr, &cr, &rxr, &ryr, &wxr, &wyr, &mr))
	{
		*x = rxr; *y = ryr;
		return 1;
	}
	return 0;
}

// for the benefit of EWMH type pagers, tag = desktop
// but, since a window can have multiple tags... oh well
unsigned int tag_to_desktop(unsigned int tag)
{
	unsigned int i; for (i = 0; i < TAGS; i++) if (tag & (1<<i)) return i;
	return 0xffffffff;
}

unsigned int desktop_to_tag(unsigned int desktop)
{
	return (desktop == 0xffffffff) ? 0: 1<<desktop;
}

// check if a window id matches a known root window
int window_is_root(Window w)
{
	int scr; for (scr = 0; scr < ScreenCount(display); scr++)
		if (RootWindow(display, scr) == w) return 1;
	return 0;
}

// XGetWindowAttributes with caching
XWindowAttributes* window_get_attributes(Window w)
{
	int idx = winlist_find(cache_xattr, w);
	if (idx < 0)
	{
		XWindowAttributes *cattr = allocate(sizeof(XWindowAttributes));
		if (XGetWindowAttributes(display, w, cattr))
		{
			winlist_append(cache_xattr, w, cattr);
			return cattr;
		}
		free(cattr);
		return NULL;
	}
	return cache_xattr->data[idx];
}

// retrieve a property of any type from a window
int window_get_prop(Window w, Atom prop, Atom *type, int *items, void *buffer, int bytes)
{
	Atom _type; if (!type) type = &_type;
	int _items; if (!items) items = &_items;
	int format; unsigned long nitems, nbytes; unsigned char *ret = NULL;
	memset(buffer, 0, bytes);

	if (XGetWindowProperty(display, w, prop, 0, bytes/4, False, AnyPropertyType, type,
		&format, &nitems, &nbytes, &ret) == Success && ret && *type != None && format)
	{
		if (format ==  8) memmove(buffer, ret, MIN(bytes, nitems));
		if (format == 16) memmove(buffer, ret, MIN(bytes, nitems * sizeof(short)));
		if (format == 32) memmove(buffer, ret, MIN(bytes, nitems * sizeof(long)));
		*items = (int)nitems; XFree(ret);
		return 1;
	}
	return 0;
}

// retrieve a text property from a window
// technically we could use window_get_prop(), but this is better for character set support
char* window_get_text_prop(Window w, Atom atom)
{
	XTextProperty prop; char *res = NULL;
	char **list = NULL; int count;
	if (XGetTextProperty(display, w, &prop, atom) && prop.value && prop.nitems)
	{
		if (prop.encoding == XA_STRING)
		{
			res = allocate(strlen((char*)prop.value)+1);
			strcpy(res, (char*)prop.value);
		}
		else
		if (XmbTextPropertyToTextList(display, &prop, &list, &count) >= Success && count > 0 && *list)
		{
			res = allocate(strlen(*list)+1);
			strcpy(res, *list);
			XFreeStringList(list);
		}
	}
	if (prop.value) XFree(prop.value);
	return res;
}

int window_get_atom_prop(Window w, Atom atom, Atom *list, int count)
{
	Atom type; int items;
	return window_get_prop(w, atom, &type, &items, list, count*sizeof(Atom)) && type == XA_ATOM ? items:0;
}

void window_set_atom_prop(Window w, Atom prop, Atom *atoms, int count)
{
	XChangeProperty(display, w, prop, XA_ATOM, 32, PropModeReplace, (unsigned char*)atoms, count);
}

int window_get_cardinal_prop(Window w, Atom atom, unsigned long *list, int count)
{
	Atom type; int items;
	return window_get_prop(w, atom, &type, &items, list, count*sizeof(unsigned long)) && type == XA_CARDINAL ? items:0;
}

void window_set_cardinal_prop(Window w, Atom prop, unsigned long *values, int count)
{
	XChangeProperty(display, w, prop, XA_CARDINAL, 32, PropModeReplace, (unsigned char*)values, count);
}

void window_unset_prop(Window w, Atom prop)
{
	XDeleteProperty(display, w, prop);
}

// a ClientMessage
// some things, like the built-in window switcher, use an EWMH ClientMessage
// also, older WM_PROTOCOLS type stuff calls this
int window_send_message(Window target, Window subject, Atom atom, unsigned long protocol, unsigned long mask)
{
	XEvent e; memset(&e, 0, sizeof(XEvent));
	e.xclient.type = ClientMessage;
	e.xclient.message_type = atom;     e.xclient.window    = subject;
	e.xclient.data.l[0]    = protocol; e.xclient.data.l[1] = CurrentTime;
	e.xclient.send_event   = True;     e.xclient.format    = 32;
	int r = XSendEvent(display, target, False, mask, &e) ?1:0;
	XFlush(display);
	return r;
}

// top-level, visible windows. DOES include non-managable docks/panels
winlist* windows_in_play(Window root)
{
	int idx = winlist_find(cache_inplay, root);
	if (idx >= 0) return cache_inplay->data[idx];

	winlist *l = winlist_new();
	unsigned int nwins; int i; Window w1, w2, *wins;
	if (XQueryTree(display, root, &w1, &w2, &wins, &nwins) && wins)
	{
		for (i = 0; i < nwins; i++)
		{
			XWindowAttributes *attr = window_get_attributes(wins[i]);
			if (attr && attr->override_redirect == False && attr->map_state == IsViewable)
				winlist_append(l, wins[i], NULL);
		}
	}
	if (wins) XFree(wins);
	winlist_append(cache_inplay, root, l);
	return l;
}

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
		if (info) for (i = 0; i < monitors; i++)
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
			unsigned long *strut, c, d; Atom a; int b; unsigned char *res;
			if (XGetWindowProperty(display, win, netatoms[_NET_WM_STRUT_PARTIAL], 0L, 12,
				False, XA_CARDINAL, &a, &b, &c, &d, &res) == Success && res)
			{
				// we only pay attention to the first four params
				// this is no more complex that _NET_WM_STRUT, but newer stuff uses _PARTIAL
				strut = (unsigned long*)res;
				left   = MAX(left, strut[0]); right  = MAX(right,  strut[1]);
				top    = MAX(top,  strut[2]); bottom = MAX(bottom, strut[3]);
				XFree(res);
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

// manipulate client->state
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
void client_set_state(client *c, Atom state, int on)
{
	if (on) client_add_state(c, state); else client_remove_state(c, state);
}
void client_toggle_state(client *c, Atom state)
{
	client_set_state(c, state, !client_has_state(c, state));
}

// the window on top of windows_activated list was the last one we activated
// assume this is still the active one... seems to work most of the time!
// if this is wrong, worst case scenario is focus manages to revert to root
int window_is_active(Window w)
{
	return windows_activated->len && w == windows_activated->array[windows_activated->len-1] ?1:0;
}

// collect info on any window
// doesn't have to be a window we'll end up managing
client* window_client(Window win)
{
	if (win == None) return NULL;
	int idx = winlist_find(cache_client, win);
	if (idx >= 0) return cache_client->data[idx];

	// if this fails, we're up that creek
	XWindowAttributes *attr = window_get_attributes(win);
	if (!attr) return NULL;

	client *c = allocate_clear(sizeof(client));
	c->window = win;
	// copy xattr so we don't have to care when stuff is freed
	memmove(&c->xattr, attr, sizeof(XWindowAttributes));
	XGetTransientForHint(display, win, &c->trans);

	c->visible = c->xattr.map_state == IsViewable ?1:0;
	c->states  = window_get_atom_prop(win, netatoms[_NET_WM_STATE], c->state, CLIENTSTATE);
	window_get_atom_prop(win, netatoms[_NET_WM_WINDOW_TYPE], &c->type, 1);

	if (c->type == None) c->type = (c->trans != None)
		// trasients default to dialog
		? netatoms[_NET_WM_WINDOW_TYPE_DIALOG]
		// non-transients default to normal
		: netatoms[_NET_WM_WINDOW_TYPE_NORMAL];

	c->manage = c->xattr.override_redirect == False
		&& c->type != netatoms[_NET_WM_WINDOW_TYPE_DESKTOP]
		&& c->type != netatoms[_NET_WM_WINDOW_TYPE_DOCK]
		&& c->type != netatoms[_NET_WM_WINDOW_TYPE_SPLASH]
		?1:0;

	c->active = c->manage && c->visible && window_is_active(c->window) ?1:0;

	// focus seems a really dodgy way to determine the "active" window, but in some
	// cases checking both ->active and ->focus is necessary to bahave logically
	Window focus; int rev;
	XGetInputFocus(display, &focus, &rev);
	c->focus = focus == win ? 1:0;

	XWMHints *hints = XGetWMHints(display, win);
	if (hints)
	{
		c->input = hints && hints->flags & InputHint ? 1: 0;
		XFree(hints);
	}
	// find last known state
	idx = winlist_find(windows, c->window);
	if (idx < 0)
	{
		wincache *cache = allocate_clear(sizeof(wincache));
		winlist_append(windows, c->window, cache);
		idx = windows->len-1;
	}
	// the cache is not tightly linked to the window at all
	// if it's populated, it gets used to make behaviour appear logically
	// if it's empty, nothing cares that much
	c->cache = windows->data[idx];

	winlist_append(cache_client, c->window, c);
	return c;
}

// extend client data
void client_descriptive_data(client *c)
{
	if (!c || c->title[0] || c->class[0]) return;

	char *name;
	if ((name = window_get_text_prop(c->window, netatoms[_NET_WM_NAME])) && name)
	{
		snprintf(c->title, CLIENTTITLE, "%s", name);
		free(name);
	}
	else
	if (XFetchName(display, c->window, &name))
	{
		snprintf(c->title, CLIENTTITLE, "%s", name);
		XFree(name);
	}
	XClassHint chint;
	if (XGetClassHint(display, c->window, &chint))
	{
		snprintf(c->class, CLIENTCLASS, "%s", chint.res_class);
		snprintf(c->name, CLIENTNAME, "%s", chint.res_name);
		XFree(chint.res_class); XFree(chint.res_name);
	}
}

// extend client data
// necessary for anything that is going to move/resize/stack, but expensive to do
// every time in window_client()
void client_extended_data(client *c)
{
	if (!c || c->w || c->h) return;

	long sr; XGetWMNormalHints(display, c->window, &c->xsize, &sr);
	monitor_dimensions_struts(c->xattr.screen, c->xattr.x, c->xattr.y, &c->monitor);

	int screen_x = c->monitor.x, screen_y = c->monitor.y;
	int screen_width = c->monitor.w, screen_height = c->monitor.h;
	int vague = screen_width/100;

	// window co-ords translated to 0-based on screen
	int x = c->xattr.x - screen_x; int y = c->xattr.y - screen_y;
	int w = c->xattr.width; int h = c->xattr.height;

	// co-ords are x,y upper left outsize border, w,h inside border
	// correct to include border in w,h for non-fullscreen windows to simplify calculations
	if (w < screen_width || h < screen_height) { w += config_border_width*2; h += config_border_width*2; }

	c->x = c->xattr.x; c->y = c->xattr.y; c->w = c->xattr.width; c->h = c->xattr.height;
	c->sx = x; c->sy = y; c->sw = w; c->sh = h;

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
}

#ifdef DEBUG
void event_log(const char *e, Window w)
{
	XClassHint chint;
	fprintf(stderr, "\n%s: %x", e, (unsigned int)w);
	if (w != None && XGetClassHint(display, w, &chint))
	{
		fprintf(stderr, " %s", chint.res_class);
		XFree(chint.res_class); XFree(chint.res_name);
	}
	fprintf(stderr, "\n");
	fflush(stderr);
}
#else
#define event_log(...)
#endif

#ifdef DEBUG
void event_note(const char *fmt, ...)
{
	fprintf(stderr, "\t");
	va_list ap; va_start(ap,fmt); vfprintf(stderr, fmt, ap); va_end(ap);
	fprintf(stderr, "\n");
}
#else
#define event_note(...)
#endif

#ifdef DEBUG
// debug
void event_client_dump(client *c)
{
	if (!c) return;
	client_descriptive_data(c);
	client_extended_data(c);
	event_note("%x title: %s", (unsigned int)c->window, c->title);
	event_note("manage:%d input:%d focus:%d", c->manage, c->input, c->focus);
	event_note("class: %s name: %s", c->class, c->name);
	event_note("x:%d y:%d w:%d h:%d b:%d override:%d transient:%x", c->xattr.x, c->xattr.y, c->xattr.width, c->xattr.height,
		c->xattr.border_width, c->xattr.override_redirect ?1:0, (unsigned int)c->trans);
	event_note("is_full:%d is_left:%d is_top:%d is_right:%d is_bottom:%d\n\t\t is_xcenter:%d is_ycenter:%d is_maxh:%d is_maxv:%d",
		c->is_full, c->is_left, c->is_top, c->is_right, c->is_bottom, c->is_xcenter, c->is_ycenter, c->is_maxh, c->is_maxv);
	int i, j;
	for (i = 0; i < NETATOMS; i++) if (c->type  == netatoms[i]) event_note("type:%s", netatom_names[i]);
	for (i = 0; i < NETATOMS; i++) for (j = 0; j < c->states; j++) if (c->state[j] == netatoms[i]) event_note("state:%s", netatom_names[i]);
	fflush(stdout);
}
#else
#define event_client_dump(...)
#endif

// update _NET_CLIENT_LIST
void ewmh_client_list(Window root)
{
	XSync(display, False);
	// this often happens after we've made changes. refresh
	winlist_empty_2d(cache_inplay);

	winlist *relevant = winlist_new();
	winlist *mapped   = winlist_new();
	int i; Window w; client *c;

	// windows_in_play() returns the stacking order. windows_activated *MAY NOT* have the same order
	winlist_ascend(windows_in_play(root), i, w)
		if ((c = window_client(w)) && c->manage && c->visible && !client_has_state(c, netatoms[_NET_WM_STATE_SKIP_TASKBAR]))
			winlist_append(relevant, w, NULL);
	XChangeProperty(display, root, netatoms[_NET_CLIENT_LIST_STACKING], XA_WINDOW, 32, PropModeReplace, (unsigned char*)relevant->array, relevant->len);

	// 'windows' list has mapping order of everything. build 'mapped' from 'relevant', ordered by 'windows'
	winlist_ascend(windows, i, w) if (winlist_forget(relevant, w)) winlist_append(mapped, w, NULL);
	XChangeProperty(display, root, netatoms[_NET_CLIENT_LIST], XA_WINDOW, 32, PropModeReplace, (unsigned char*)mapped->array, mapped->len);

	winlist_free(mapped);
	winlist_free(relevant);
}

// update _NET_ACTIVE_WINDOW
void ewmh_active_window(Window root, Window w)
{
	XChangeProperty(display, root, netatoms[_NET_ACTIVE_WINDOW], XA_WINDOW, 32, PropModeReplace, (unsigned char*)&w, 1);
}

// _NET_DESKTOP stuff, taking _NET_WM_STRUT* into account
void ewmh_desktop_list(Window root)
{
	int i; XWindowAttributes *attr = window_get_attributes(root);
	// nine desktops. want more space? buy more monitors and use xinerama :)
	unsigned long desktops = TAGS, area[4*TAGS], geo[2], view[2], desktop;

	// this will return the full X screen, not Xinerama screen
	workarea mon; monitor_dimensions_struts(attr->screen, -1, -1, &mon);

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
	if (c->cache->have_closed || !client_protocol_event(c, atoms[WM_DELETE_WINDOW]))
		XKillClient(display, c->window);
	c->cache->have_closed = 1;
}

// true if x/y is over a visible portion of the client window
int client_warp_check(client *c, int x, int y)
{
	int i, ok = 1; Window w; client *o;
	winlist_descend(windows_in_play(c->xattr.root), i, w)
	{
		if (!ok || w == c->window) break;
		if ((o = window_client(w)) && o->manage && o->visible
			&& INTERSECT(o->xattr.x, o->xattr.y, o->xattr.width, o->xattr.height, x, y, 1, 1))
				ok = 0;
	}
	return ok;
}

// ensure the pointer is over a specific client
void client_warp_pointer(client *c)
{
	// needs the updated stacking mode, so clear cache
	XSync(display, False);
	winlist_empty_2d(cache_inplay);

	client_extended_data(c);
	int vague = c->monitor.w/100;
	int x, y; pointer_get(c->xattr.root, &x, &y);
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
		event_note("%d %d", xd, yd);
		// step toward client window
		while ((xd || yd ) && INTERSECT(c->x, c->y, c->w, c->h, x, y, 1, 1) && !client_warp_check(c, x, y))
			{ x += xd; y += yd; }
	}
	// ensure pointer is slightly inside border
	x = MIN(c->x+c->w-vague, MAX(c->x+vague, x));
	y = MIN(c->y+c->h-vague, MAX(c->y+vague, y));
	XWarpPointer(display, None, None, 0, 0, 0, 0, x-mx, y-my);
}

// move & resize a window nicely
void client_moveresize(client *c, int smart, int fx, int fy, int fw, int fh)
{
	client_extended_data(c);

	// this many be different to the client's current c->monitor...
	workarea monitor; monitor_dimensions_struts(c->xattr.screen, fx, fy, &monitor);

	// ensure we match maxv/maxh mode
	if (client_has_state(c, netatoms[_NET_WM_STATE_MAXIMIZED_HORZ]))
		{ fx = monitor.x; fw = monitor.w; }
	if (client_has_state(c, netatoms[_NET_WM_STATE_MAXIMIZED_VERT]))
		{ fy = monitor.y; fh = monitor.h; }

	// process size hints
	if (c->xsize.flags & PMinSize)
	{
		fw = MAX(fw, c->xsize.min_width);
		fh = MAX(fh, c->xsize.min_height);
	}
	if (c->xsize.flags & PMaxSize)
	{
		fw = MIN(fw, c->xsize.max_width);
		fh = MIN(fh, c->xsize.max_height);
	}
	if (c->xsize.flags & PAspect)
	{
		double ratio = (double) fw / fh;
		double minr = (double) c->xsize.min_aspect.x / c->xsize.min_aspect.y;
		double maxr = (double) c->xsize.max_aspect.x / c->xsize.max_aspect.y;
			if (ratio < minr) fh = (int)(fw / minr);
		else if (ratio > maxr) fw = (int)(fh * maxr);
	}
	// bump onto screen. shrink if necessary
	fw = MAX(1, MIN(fw, monitor.w)); fh = MAX(1, MIN(fh, monitor.h));
	fx = MAX(MIN(fx, monitor.x + monitor.w - fw), monitor.x);
	fy = MAX(MIN(fy, monitor.y + monitor.h - fh), monitor.y);
	//fw = MAX(1, MIN(fw, monitor.w - fx + monitor.x));
	//fh = MAX(1, MIN(fh, monitor.h - fy + monitor.y));
	// put the window in same general position it was before
	if (smart)
	{
		// shrinking w. check if we were once in a corner previous-to-last
		// expanding w is already covered by bumping above
		if (c->cache && c->cache->last_corner && c->sw > fw)
		{
			if (c->cache->last_corner == TOPLEFT || c->cache->last_corner == BOTTOMLEFT)
				fx = monitor.x;
			if (c->cache->last_corner == TOPRIGHT || c->cache->last_corner == BOTTOMRIGHT)
				fx = monitor.x + monitor.w - fw;
		}
		// screen center always wins
		else if (c->is_xcenter) fx = monitor.x + ((monitor.w - fw) / 2);
		else if (c->is_left) fx = monitor.x;
		else if (c->is_right) fx = monitor.x + monitor.w - fw;

		// shrinking h. check if we were once in a corner previous-to-last
		// expanding h is already covered by bumping above
		if (c->cache && c->cache->last_corner && c->sh > fh)
		{
			if (c->cache->last_corner == TOPLEFT || c->cache->last_corner == TOPRIGHT)
				fy = monitor.y;
			if (c->cache->last_corner == BOTTOMLEFT || c->cache->last_corner == BOTTOMRIGHT)
				fy = monitor.y + monitor.h - fh;
		}
		// screen center always wins
		else if (c->is_ycenter) fy = monitor.y + ((monitor.h - fh) / 2);
		else if (c->is_top) fy = monitor.y;
		else if (c->is_bottom) fy = monitor.y + monitor.h - fh;
	}

	// true fullscreen without struts is done in client_fullscreen()
	client_remove_state(c, netatoms[_NET_WM_STATE_FULLSCREEN]);

	// compensate for border on non-fullscreen windows
	if (fw < monitor.w || fh < monitor.h)
	{
		fw = MAX(1, fw-(config_border_width*2));
		fh = MAX(1, fh-(config_border_width*2));
	}
	XMoveResizeWindow(display, c->window, fx, fy, fw, fh);

	// track the move/resize instruction
	// apps that come back with an alternative configurerequest (eg, some terminals, gvim, etc)
	// get denied unless their hints check out
	if (c->cache)
	{
		c->cache->have_mr = 1;
		c->cache->mr_x = fx; c->cache->mr_y = fy;
		c->cache->mr_w = fw; c->cache->mr_h = fh;
	}
}

// record a window's size and position in the undo log
void client_commit(client *c)
{
	client_extended_data(c);
	winundo *undo;

	if (c->cache->undo_levels > 0)
	{
		// check if the last undo state matches current state. if so, no point recording
		undo = &c->cache->undo[c->cache->undo_levels-1];
		if (undo->x == c->x && undo->y == c->y && undo->w == c->w && undo->h == c->h) return;
	}
	// LIFO up to UNDO cells deep
	if (c->cache->undo_levels == UNDO)
	{
		memmove(c->cache->undo, &c->cache->undo[1], sizeof(winundo)*(UNDO-1));
		c->cache->undo_levels--;
	}
	undo = &c->cache->undo[c->cache->undo_levels++];
	undo->x  = c->x;  undo->y  = c->y;  undo->w  = c->w;  undo->h  = c->h;
	undo->sx = c->sx; undo->sy = c->sy; undo->sw = c->sw; undo->sh = c->sh;
	for (undo->states = 0; undo->states < c->states; undo->states++)
		undo->state[undo->states] = c->state[undo->states];
}

// move/resize a window back to it's last known size and position
void client_rollback(client *c)
{
	if (c->cache->undo_levels > 0)
	{
		winundo *undo = &c->cache->undo[--c->cache->undo_levels];
		for (c->states = 0; c->states < undo->states; c->states++)
			c->state[c->states] = undo->state[c->states];
		client_flush_state(c);
		client_moveresize(c, 0, undo->x, undo->y, undo->sw, undo->sh);
	}
}

// save co-ords for later flip-back
// these may MAY BE dulicated in the undo log, but they must remain separate
// to allow proper toggle behaviour for maxv/maxh
void client_save_position(client *c)
{
	client_extended_data(c);
	if (!c->cache) return;
	c->cache->have_old = 1;
	c->cache->x = c->x; c->cache->sx = c->sx;
	c->cache->y = c->y; c->cache->sy = c->sy;
	c->cache->w = c->w; c->cache->sw = c->sw;
	c->cache->h = c->h; c->cache->sh = c->sh;
}

// save co-ords for later flip-back
void client_save_position_horz(client *c)
{
	client_extended_data(c); if (!c->cache) return;
	if (!c->cache->have_old) client_save_position(c);
	c->cache->x = c->x; c->cache->sx = c->sx;
	c->cache->w = c->w; c->cache->sw = c->sw;
}

// save co-ords for later flip-back
void client_save_position_vert(client *c)
{
	client_extended_data(c); if (!c->cache) return;
	if (!c->cache->have_old) client_save_position(c);
	c->cache->y = c->y; c->cache->sy = c->sy;
	c->cache->h = c->h; c->cache->sh = c->sh;
}

// revert to saved co-ords
void client_restore_position(client *c, int smart, int x, int y, int w, int h)
{
	client_extended_data(c);
	client_moveresize(c, smart,
		c->cache && c->cache->have_old ? c->cache->x: x,
		c->cache && c->cache->have_old ? c->cache->y: y,
		c->cache && c->cache->have_old ? c->cache->sw: w,
		c->cache && c->cache->have_old ? c->cache->sh: h);
}

// revert to saved co-ords
void client_restore_position_horz(client *c, int smart, int x, int w)
{
	client_extended_data(c);
	client_moveresize(c, smart,
		c->cache && c->cache->have_old ? c->cache->x: x, c->y,
		c->cache && c->cache->have_old ? c->cache->sw: w, c->sh);
}

// revert to saved co-ords
void client_restore_position_vert(client *c, int smart, int y, int h)
{
	client_extended_data(c);
	client_moveresize(c, smart,
		c->x, c->cache && c->cache->have_old ? c->cache->y: y,
		c->sw, c->cache && c->cache->have_old ? c->cache->sh: h);
}

// expand a window to take up available space around it on the current monitor
// do not cover any window that is entirely visible (snap to surrounding edges)
void client_expand(client *c, int directions, int x1, int y1, int w1, int h1, int mx, int my, int mw, int mh)
{
	client_extended_data(c);

	winlist *inplay = windows_in_play(c->xattr.root);
	// list of coords/sizes for fully visible windows on this desktop
	workarea *regions = allocate_clear(sizeof(workarea) * inplay->len);
	// list of coords/sizes for all windows on this desktop
	workarea *allregions = allocate_clear(sizeof(workarea) * inplay->len);

	int i, relevant = 0; Window win; client *o;

	// build the (all)regions arrays
	winlist_descend(inplay, i, win)
	{
		if (win == c->window) continue;
		if ((o = window_client(win)) && o && o->manage && o->visible)
		{
			client_extended_data(o);
			// only concerned about windows on this monitor
			if (o->monitor.x == c->monitor.x && o->monitor.y == c->monitor.y) // same monitor
			{
				int j, obscured = 0;
				for (j = inplay->len-1; j > i; j--)
				{
					// if the window intersects with any other window higher in the stack order, it must be at least partially obscured
					if (allregions[j].w && INTERSECT(o->sx, o->sy, o->sw, o->sh,
						allregions[j].x, allregions[j].y, allregions[j].w, allregions[j].h))
							{ obscured = 1; break; }
				}
				// record a full visible window
				if (!obscured)
				{
					regions[relevant].x = o->sx; regions[relevant].y = o->sy;
					regions[relevant].w = o->sw; regions[relevant].h = o->sh;
					relevant++;
				}
				allregions[i].x = o->sx; allregions[i].y = o->sy;
				allregions[i].w = o->sw; allregions[i].h = o->sh;
			}
		}
	}

	int n, x = c->sx, y = c->sy, w = c->sw, h = c->sh;
	if (w1 || h1) { x = x1; y = y1; w = w1; h = h1; }

	if (directions & VERTICAL)
	{
		// try to grow upward. locate the lower edge of the nearest fully visible window
		for (n = 0, i = 0; i < relevant; i++)
		{
			if (regions[i].y + regions[i].h <= y && OVERLAP(x, w, regions[i].x, regions[i].w))
				n = MAX(n, regions[i].y + regions[i].h);
		}
		h += y-n; y = n;
		// try to grow downward. locate the upper edge of the nearest fully visible window
		for (n = c->monitor.h, i = 0; i < relevant; i++)
		{
			if (regions[i].y >= y+h && OVERLAP(x, w, regions[i].x, regions[i].w))
				n = MIN(n, regions[i].y);
		}
		h = n-y;
	}
	if (directions & HORIZONTAL)
	{
		// try to grow left. locate the right edge of the nearest fully visible window
		for (n = 0, i = 0; i < relevant; i++)
		{
			if (regions[i].x + regions[i].w <= x && OVERLAP(y, h, regions[i].y, regions[i].h))
				n = MAX(n, regions[i].x + regions[i].w);
		}
		w += x-n; x = n;
		// try to grow right. locate the left edge of the nearest fully visible window
		for (n = c->monitor.w, i = 0; i < relevant; i++)
		{
			if (regions[i].x >= x+w && OVERLAP(y, h, regions[i].y, regions[i].h))
				n = MIN(n, regions[i].x);
		}
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
	// if there is nowhere to grow and we have a saved position, flip back to it.
	// allows the expand key to be used as a toggle!
	if (x == c->sx && y == c->sy && w == c->sw && h == c->sh && c->cache->have_old)
	{
		if (directions & VERTICAL && directions & HORIZONTAL)
			client_restore_position(c, 0, c->x, c->y, c->cache->sw, c->cache->sh);
		else
		if (directions & VERTICAL)
			client_restore_position_vert(c, 0, c->y, c->cache->sh);
		else
		if (directions & HORIZONTAL)
			client_restore_position_horz(c, 0, c->x, c->cache->sw);
	}
	else
	{
		// save pos for toggle
		if (directions & VERTICAL && directions & HORIZONTAL)
			client_save_position(c);
		else
		if (directions & VERTICAL)
			client_save_position_vert(c);
		else
		if (directions & HORIZONTAL)
			client_save_position_horz(c);
		client_commit(c);
		client_moveresize(c, 0, c->monitor.x+x, c->monitor.y+y, w, h);
	}
	free(regions); free(allregions);
}

// shrink to fit into an empty gap underneath. opposite of client_expand()
void client_contract(client *c, int directions)
{
	client_extended_data(c);
	// cheat and shrink the window absurdly so it becomes just another expansion
	if (directions & VERTICAL && directions & HORIZONTAL)
		client_expand(c, directions, c->sx+(c->sw/2), c->sy+(c->sh/2), 5, 5, c->sx, c->sy, c->sw, c->sh);
	else
	if (directions & VERTICAL)
		client_expand(c, directions, c->sx, c->sy+(c->sh/2), c->sw, 5, c->sx, c->sy, c->sw, c->sh);
	else
	if (directions & HORIZONTAL)
		client_expand(c, directions, c->sx+(c->sw/2), c->sy, 5, c->sh, c->sx, c->sy, c->sw, c->sh);
}

// visually highlight a client to attract attention
// for now, four coloured squares in the corners. could get fancier?
void client_flash(client *c, unsigned int color, int delay)
{
	client_extended_data(c);
	if (config_flash_width > 0 && !fork())
	{
		display = XOpenDisplay(0x0);

		int x1 = c->x, x2 = c->x + c->sw - config_flash_width;
		int y1 = c->y, y2 = c->y + c->sh - config_flash_width;

		// if there is a move request dispatched, flash there to match
		if (c->cache && c->cache->have_mr)
		{
			x1 = c->cache->mr_x; x2 = x1 + c->cache->mr_w - config_flash_width + config_border_width;
			y1 = c->cache->mr_y; y2 = y1 + c->cache->mr_h - config_flash_width + config_border_width;
		}
		// four coloured squares in the window's corners
		Window tl = XCreateSimpleWindow(display, c->xattr.root, x1, y1, config_flash_width, config_flash_width, 0, None, color);
		Window tr = XCreateSimpleWindow(display, c->xattr.root, x2, y1, config_flash_width, config_flash_width, 0, None, color);
		Window bl = XCreateSimpleWindow(display, c->xattr.root, x1, y2, config_flash_width, config_flash_width, 0, None, color);
		Window br = XCreateSimpleWindow(display, c->xattr.root, x2, y2, config_flash_width, config_flash_width, 0, None, color);

		XSetWindowAttributes attr; attr.override_redirect = True;
		XChangeWindowAttributes(display, tl, CWOverrideRedirect, &attr);
		XChangeWindowAttributes(display, tr, CWOverrideRedirect, &attr);
		XChangeWindowAttributes(display, bl, CWOverrideRedirect, &attr);
		XChangeWindowAttributes(display, br, CWOverrideRedirect, &attr);

		XMapRaised(display, tl); XMapRaised(display, tr);
		XMapRaised(display, bl); XMapRaised(display, br);
		XSync(display, False);
		usleep(delay*1000);
		XDestroyWindow(display, tl); XDestroyWindow(display, tr);
		XDestroyWindow(display, bl); XDestroyWindow(display, br);
		exit(EXIT_SUCCESS);
	}
}

// add a window and family to the stacking order
void client_stack_family(client *c, winlist *stack)
{
	int i; client *a = NULL;
	Window orig = c->window, app = orig;

	// if this is a transient window, find the main app
	// TODO: this doesn't handle multiple transient levels, like Gimp's save/export sequence
	if (c->trans)
	{
		a = window_client(c->trans);
		if (a && a->manage) app = a->window;
	}

	if (app != orig) winlist_append(stack, orig, NULL);

	// locate all visible transient windows for this app
	winlist *inplay = windows_in_play(c->xattr.root);
	for (i = inplay->len-1; i > -1; i--)
	{
		if (inplay->array[i] == app) continue;
		a = window_client(inplay->array[i]);
		if (a && a->trans == app) winlist_append(stack, a->window, NULL);
	}
	winlist_append(stack, app, NULL);
}

// raise a window and its transients
void client_raise(client *c, int priority)
{
	int i; Window w; client *o;
	winlist *stack = winlist_new();
	winlist *inplay = windows_in_play(c->xattr.root);

	if (!priority && client_has_state(c, netatoms[_NET_WM_STATE_BELOW]))
		return;

	// priority gets us raised without anyone above us, regardless. eg _NET_WM_STATE_FULLSCREEN+focus
	if (!priority)
	{
		// if we're above, ensure it
		// allows cycling between multiple _NET_WM_STATE_ABOVE windows, regardless of their original mapping order
		if (client_has_state(c, netatoms[_NET_WM_STATE_ABOVE]))
			client_stack_family(c, stack);

		// locate windows with both _NET_WM_STATE_STICKY and _NET_WM_STATE_ABOVE
		winlist_descend(inplay, i, w)
		{
			if (winlist_find(stack, w) < 0 && (o = window_client(w)) && o && o->visible
				&& o->trans == None && client_has_state(o, netatoms[_NET_WM_STATE_ABOVE])
				&& client_has_state(o, netatoms[_NET_WM_STATE_STICKY]))
					client_stack_family(o, stack);
		}
		// locate windows in the current_tag with _NET_WM_STATE_ABOVE
		// untagged windows with _NET_WM_STATE_ABOVE are effectively sticky
		winlist_descend(inplay, i, w)
		{
			if (winlist_find(stack, w) < 0 && (o = window_client(w)) && o && o->visible
				&& o->trans == None && client_has_state(o, netatoms[_NET_WM_STATE_ABOVE])
				&& (!o->cache->tags || current_tag & o->cache->tags))
					client_stack_family(o, stack);
		}
		// locate _NET_WM_WINDOW_TYPE_DOCK windows
		winlist_descend(inplay, i, w)
		{
			if (winlist_find(stack, w) < 0 && (o = window_client(w)) && o && o->visible
				&& c->trans == None && o->type == netatoms[_NET_WM_WINDOW_TYPE_DOCK])
					client_stack_family(o, stack);
		}
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

// raise a window and its transients
void client_lower(client *c, int priority)
{
	int i; Window w; client *o;
	winlist *stack = winlist_new();
	winlist *inplay = windows_in_play(c->xattr.root);

	if (!priority && client_has_state(c, netatoms[_NET_WM_STATE_ABOVE]))
		return;

	if (priority)
		client_stack_family(c, stack);

	// locate windows in the current_tag with _NET_WM_STATE_BELOW
	// untagged windows with _NET_WM_STATE_BELOW are effectively sticky
	winlist_descend(inplay, i, w)
	{
		if (winlist_find(stack, w) < 0 && (o = window_client(w)) && o && o->visible
			&& o->trans == None && client_has_state(o, netatoms[_NET_WM_STATE_BELOW])
			&& (!o->cache->tags || current_tag & o->cache->tags))
				client_stack_family(o, stack);
	}
	// locate windows with both _NET_WM_STATE_STICKY and _NET_WM_STATE_BELOW
	winlist_descend(inplay, i, w)
	{
		if (winlist_find(stack, w) < 0 && (o = window_client(w)) && o && o->visible
			&& o->trans == None && client_has_state(o, netatoms[_NET_WM_STATE_BELOW])
			&& client_has_state(o, netatoms[_NET_WM_STATE_STICKY]))
				client_stack_family(o, stack);
	}

	if (winlist_find(stack, c->window) < 0)
		client_stack_family(c, stack);

	// raise the top window in the stack
	XLowerWindow(display, stack->array[stack->len-1]);
	// stack everything else, in order, underneath top window
	if (stack->len > 1) XRestackWindows(display, stack->array, stack->len);

	winlist_free(stack);
}

// set border width approriate to position and size
void client_review_border(client *c)
{
	client_extended_data(c);
	XSetWindowBorderWidth(display, c->window, c->is_full ? 0:config_border_width);
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
		if (c->is_left  && c->is_top) c->cache->last_corner = TOPLEFT;
		else if (c->is_left  && c->is_bottom) c->cache->last_corner = BOTTOMLEFT;
		else if (c->is_right && c->is_top) c->cache->last_corner = TOPRIGHT;
		else if (c->is_right && c->is_bottom) c->cache->last_corner = BOTTOMRIGHT;
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

// update client border to blurred
void client_deactivate(client *c)
{
	XSetWindowBorder(display, c->window, client_has_state(c, netatoms[_NET_WM_STATE_DEMANDS_ATTENTION])
		? config_border_attention: config_border_blur);
}

// raise and focus a client
void client_activate(client *c, int raise, int warp)
{
	int i; Window w; client *o;

	// deactivate everyone else
	winlist_ascend(windows_in_play(c->xattr.root), i, w)
		if (w != c->window && (o = window_client(w)) && o->manage) client_deactivate(o);

	// setup ourself
	if (config_raise_mode == RAISEFOCUS || raise)
		client_raise(c, client_has_state(c, netatoms[_NET_WM_STATE_FULLSCREEN]));

	// focus a window politely if possible
	client_protocol_event(c, atoms[WM_TAKE_FOCUS]);
	if (c->input) XSetInputFocus(display, c->window, RevertToPointerRoot, CurrentTime);
	else XSetInputFocus(display, PointerRoot, RevertToPointerRoot, CurrentTime);
	XSetWindowBorder(display, c->window, config_border_focus);

	// we have recieved attention
	client_remove_state(c, netatoms[_NET_WM_STATE_DEMANDS_ATTENTION]);

	// update focus history order
	winlist_forget(windows_activated, c->window);
	winlist_append(windows_activated, c->window, NULL);
	ewmh_active_window(c->xattr.root, c->window);

	// tell the user something happened
	if (!c->active) client_flash(c, config_border_focus, config_flash_ms);

	// must happen last, after all move/resize/focus/raise stuff is sent
	if (config_warp_mode == WARPFOCUS || warp)
		client_warp_pointer(c);
}

// set WM_STATE
void client_state(client *c, long state)
{
	long payload[] = { state, None };
	XChangeProperty(display, c->window, atoms[WM_STATE], atoms[WM_STATE], 32, PropModeReplace, (unsigned char*)payload, 2);
	if (state == NormalState)
		client_full_review(c);
	else
	if (state == WithdrawnState)
	{
		window_unset_prop(c->window, netatoms[_NET_WM_STATE]);
		window_unset_prop(c->window, netatoms[_NET_WM_DESKTOP]);
		winlist_forget(windows_activated, c->window);
	}
}

// locate the currently focused window and build a client for it
client* window_active_client(Window root, unsigned int tag)
{
	int i; Window w; client *c = NULL;
	// look for a visible, previously activated window in the current tag
	if (tag) winlist_descend(windows_activated, i, w)
		if ((c = window_client(w)) && c && c->manage && c->visible
			&& c->cache->tags & tag && c->xattr.root == root) break;
	// look for a visible, previously activated window anywhere
	if (!c) winlist_descend(windows_activated, i, w)
		if ((c = window_client(w)) && c && c->manage && c->visible
			&& c->xattr.root == root) break;
	// if we found one, activate it
	if (c && (!c->focus || !c->active))
		client_activate(c, RAISEDEF, WARPDEF);
	// otherwise look for any visible, manageable window
	if (!c)
	{
		winlist_descend(windows_in_play(root), i, w)
			if ((c = window_client(w)) && c && c->manage && c->visible) break;
		if (c) client_activate(c, RAISEDEF, WARPDEF);
	}
	return c;
}

// raise all windows in a tag
void tag_raise(unsigned int tag)
{
	int i; Window w; client *c;
	winlist *stack; Window focus;

	int scr; for (scr = 0; scr < ScreenCount(display); scr++)
	{
		Window root = RootWindow(display, scr);
		winlist *inplay = windows_in_play(root);
		stack = winlist_new();
		focus = None;

		// locate windows with _NET_WM_STATE_ABOVE and _NET_WM_STATE_STICKY
		winlist_descend(inplay, i, w)
		{
			if (winlist_find(stack, w) < 0 && (c = window_client(w)) && c && c->visible
				&& c->trans == None && client_has_state(c, netatoms[_NET_WM_STATE_ABOVE])
				&& client_has_state(c, netatoms[_NET_WM_STATE_STICKY]))
					client_stack_family(c, stack);
		}
		// locate windows with _NET_WM_STATE_ABOVE in this tag
		winlist_descend(inplay, i, w)
		{
			if (winlist_find(stack, w) < 0 && (c = window_client(w)) && c && c->visible
				&& c->trans == None && client_has_state(c, netatoms[_NET_WM_STATE_ABOVE]) && c->cache->tags & tag)
					client_stack_family(c, stack);
		}
		// locate _NET_WM_WINDOW_TYPE_DOCK windows
		winlist_descend(inplay, i, w)
		{
			if (winlist_find(stack, w) < 0 && (c = window_client(w)) && c && c->visible
				&& c->trans == None && c->type == netatoms[_NET_WM_WINDOW_TYPE_DOCK])
					client_stack_family(c, stack);
		}
		// locate all other windows in the tag, and the top one to be focused
		winlist_descend(inplay, i, w)
		{
			if (winlist_find(stack, w) < 0 && (c = window_client(w)) && c && c->manage && c->visible
				&& c->trans == None && c->cache->tags & tag)
			{
				if (focus == None) focus = w;
				client_stack_family(c, stack);
			}
		}
		if (stack->len)
		{
			// raise the top window in the stack
			XRaiseWindow(display, stack->array[0]);
			// stack everything else, in order, underneath top window
			if (stack->len > 1) XRestackWindows(display, stack->array, stack->len);
		}
		winlist_free(stack);

		// update current desktop on all roots
		current_tag = tag; unsigned long d = tag_to_desktop(current_tag);
		window_set_cardinal_prop(RootWindow(display, scr), netatoms[_NET_CURRENT_DESKTOP], &d, 1);

		// focus the top-most non-_NET_WM_STATE_ABOVE managable client in the tag
		if (focus != None)
		{
			client *c = window_client(focus);
			if (c) client_activate(c, RAISEDEF, WARPDEF);
		}
	}
}

// toggle client in current tag
void client_toggle_tag(client *c, unsigned int tag, int flash)
{
	if (c->cache->tags & tag)
	{
		c->cache->tags &= ~tag;
		if (flash) client_flash(c, config_flash_off, config_flash_ms);
	}
	else
	{
		c->cache->tags |= tag;
		if (flash) client_flash(c, config_flash_on, config_flash_ms);
	}
	// update _NET_WM_DESKTOP using lowest tag number.
	// this is a bit of a fudge as we can have windows on multiple
	// tags/desktops, without being specifically sticky... oh well.
	unsigned long d = tag_to_desktop(c->cache->tags);
	window_set_cardinal_prop(c->window, netatoms[_NET_WM_DESKTOP], &d, 1);
	ewmh_client_list(c->xattr.root);
}

// determine which monitor holds the active window, or failing that the mouse pointer
void monitor_active(Screen *screen, workarea *mon)
{
	Window root = RootWindow(display, XScreenNumberOfScreen(screen));
	client *c = window_active_client(root, 0);
	if (c)
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

// go fullscreen on current monitor
void client_nws_fullscreen(client *c, int action)
{
	int state = client_has_state(c, netatoms[_NET_WM_STATE_FULLSCREEN]);

	if (action == ADD || (action == TOGGLE && !state))
	{
		client_commit(c);
		client_save_position(c);
		// no struts!
		workarea monitor; monitor_dimensions(c->xattr.screen, c->xattr.x, c->xattr.y, &monitor);
		client_set_state(c, netatoms[_NET_WM_STATE_FULLSCREEN], 1);
		// not client_moveresize! that would get tricky and recheck struts
		XMoveResizeWindow(display, c->window, monitor.x, monitor.y, monitor.w, monitor.h);
	}
	else
	if (action == REMOVE || (action == TOGGLE && state))
	{
		client_extended_data(c);
		client_commit(c);
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
		client_flash(c, config_flash_on, config_flash_ms);
	}
	else
	if (action == REMOVE || (action == TOGGLE && state))
	{
		client_remove_state(c, netatoms[_NET_WM_STATE_ABOVE]);
		client_flash(c, config_flash_off, config_flash_ms);
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
		client_flash(c, config_flash_on, config_flash_ms);
	}
	else
	if (action == REMOVE || (action == TOGGLE && state))
	{
		client_remove_state(c, netatoms[_NET_WM_STATE_BELOW]);
		client_flash(c, config_flash_off, config_flash_ms);
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
		client_flash(c, config_flash_on, config_flash_ms);
	}
	else
	if (action == REMOVE || (action == TOGGLE && state))
	{
		client_remove_state(c, netatoms[_NET_WM_STATE_STICKY]);
		client_flash(c, config_flash_off, config_flash_ms);
	}
}

// maximize vertically
void client_nws_maxvert(client *c, int action)
{
	client_extended_data(c);
	int state = client_has_state(c, netatoms[_NET_WM_STATE_MAXIMIZED_VERT]);

	if (action == ADD || (action == TOGGLE && !state))
	{
		client_commit(c);
		client_save_position_vert(c);
		client_add_state(c, netatoms[_NET_WM_STATE_MAXIMIZED_VERT]);
		client_moveresize(c, 1, c->x, c->y, c->sw, c->monitor.h);
		client_flash(c, config_flash_on, config_flash_ms);
	}
	else
	if (action == REMOVE || (action == TOGGLE && state))
	{
		client_commit(c);
		client_remove_state(c, netatoms[_NET_WM_STATE_MAXIMIZED_VERT]);
		client_restore_position_vert(c, 0, c->monitor.y + (c->monitor.h/4), c->monitor.h/2);
		client_flash(c, config_flash_off, config_flash_ms);
	}
}

// maximize horizontally
void client_nws_maxhorz(client *c, int action)
{
	client_extended_data(c);
	int state = client_has_state(c, netatoms[_NET_WM_STATE_MAXIMIZED_HORZ]);

	if (action == ADD || (action == TOGGLE && !state))
	{
		client_commit(c);
		client_save_position_horz(c);
		client_add_state(c, netatoms[_NET_WM_STATE_MAXIMIZED_HORZ]);
		client_moveresize(c, 1, c->x, c->y, c->monitor.w, c->sh);
		client_flash(c, config_flash_on, config_flash_ms);
	}
	else
	if (action == REMOVE || (action == TOGGLE && state))
	{
		client_commit(c);
		client_remove_state(c, netatoms[_NET_WM_STATE_MAXIMIZED_HORZ]);
		client_restore_position_horz(c, 0, c->monitor.x + (c->monitor.w/4), c->monitor.w/2);
		client_flash(c, config_flash_off, config_flash_ms);
	}
}

// review client's position and size when the environmetn has changed (eg, STRUT changes)
void client_nws_review(client *c)
{
	client_extended_data(c);
	int commit = 0;
	if (client_has_state(c, netatoms[_NET_WM_STATE_MAXIMIZED_HORZ]))
	{
		client_moveresize(c, 1, c->x, c->y, c->monitor.w, c->sh);
		commit = 1;
	}
	if (client_has_state(c, netatoms[_NET_WM_STATE_MAXIMIZED_VERT]))
	{
		client_moveresize(c, 1, c->x, c->y, c->sw, c->monitor.h);
		commit = 1;
	}
	if (commit) client_commit(c);
}

// cycle through tag windows in roughly the same screen position and tag
void client_cycle(client *c)
{
	client_extended_data(c);
	int i, vague = c->monitor.w/100; Window w; client *o;
	winlist_ascend(windows_in_play(c->xattr.root), i, w)
	{
		if (w != c->window && (o = window_client(w)) && o && o->manage && o->visible && c->cache->tags & o->cache->tags)
		{
			client_extended_data(o);
			if (NEAR(c->x, vague, o->x) &&
				NEAR(c->y, vague, o->y) &&
				NEAR(c->w, vague, o->w) &&
				NEAR(c->h, vague, o->h))
			{
				client_activate(o, RAISE, WARPDEF);
				break;
			}
		}
	}
}

// horizontally tile two windows in the same screen position and tag
void client_htile(client *c)
{
	client_extended_data(c);
	winlist *tiles = winlist_new();
	winlist_append(tiles, c->window, NULL);
	int i, vague = c->monitor.w/100; Window w; client *o;
	winlist_descend(windows_in_play(c->xattr.root), i, w)
	{
		if (w != c->window && (o = window_client(w)) && o && c->manage && o->visible && c->cache->tags & o->cache->tags)
		{
			client_extended_data(o);
			if (NEAR(c->x, vague, o->x) &&
				NEAR(c->y, vague, o->y) &&
				NEAR(c->w, vague, o->w) &&
				NEAR(c->h, vague, o->h))
					winlist_append(tiles, w, NULL);
		}
	}
	if (tiles->len > 1)
	{
		int width = c->sw / tiles->len;
		winlist_ascend(tiles, i, w)
		{
			o = window_client(w);
			client_commit(o);
			client_remove_state(o, netatoms[_NET_WM_STATE_MAXIMIZED_HORZ]);
			client_remove_state(o, netatoms[_NET_WM_STATE_MAXIMIZED_VERT]);
			client_moveresize(o, 0, c->x+(width*i), c->y, width, c->sh);
		}
	}
	// nothing to tile with. still make a gap for something subsequent
	else
	{
		client_commit(c);
		client_moveresize(c, 0, c->x, c->y, c->sw/2, c->sh);
	}
	winlist_free(tiles);
}

// vertically tile two windows in the same screen position and tag
void client_vtile(client *c)
{
	client_extended_data(c);
	winlist *tiles = winlist_new();
	winlist_append(tiles, c->window, NULL);
	int i, vague = c->monitor.w/100; Window w; client *o;
	winlist_descend(windows_in_play(c->xattr.root), i, w)
	{
		if (w != c->window && (o = window_client(w)) && o && c->manage && o->visible && c->cache->tags & o->cache->tags)
		{
			client_extended_data(o);
			if (NEAR(c->x, vague, o->x) &&
				NEAR(c->y, vague, o->y) &&
				NEAR(c->w, vague, o->w) &&
				NEAR(c->h, vague, o->h))
					winlist_append(tiles, w, NULL);
		}
	}
	if (tiles->len > 1)
	{
		int height = c->sh / tiles->len;
		winlist_ascend(tiles, i, w)
		{
			o = window_client(w);
			client_commit(o);
			client_remove_state(o, netatoms[_NET_WM_STATE_MAXIMIZED_HORZ]);
			client_remove_state(o, netatoms[_NET_WM_STATE_MAXIMIZED_VERT]);
			client_moveresize(o, 0, c->x, c->y+(height*i), c->sw, height);
		}
	}
	// nothing to tile with. still make a gap for something subsequent
	else
	{
		client_commit(c);
		client_moveresize(c, 0, c->x, c->y, c->sw, c->sh/2);
	}
	winlist_free(tiles);
}

// move focus by direction. this is a visual thing, not restricted by tag
void client_focusto(client *c, int direction)
{
	client_extended_data(c);
	int i, vague = c->monitor.w/100; Window w; client *o;
	// look for a window immediately adjacent or overlapping
	winlist_descend(windows_in_play(c->xattr.root), i, w)
	{
		if ((o = window_client(w)) && o && o->manage && o->visible)
		{
			client_extended_data(o);
			if ((direction == FOCUSLEFT  && o->x < c->x
					&& INTERSECT(c->x-vague, c->y, c->sw+vague, c->sh, o->x, o->y, o->sw, o->sh)) ||
				(direction == FOCUSRIGHT && o->x+o->w > c->x+c->w
					&& INTERSECT(c->x, c->y, c->sw+vague, c->sh, o->x, o->y, o->sw, o->sh)) ||
				(direction == FOCUSUP    && o->y < c->y
					&& INTERSECT(c->x, c->y-vague, c->sw, c->sh+vague, o->x, o->y, o->sw, o->sh)) ||
				(direction == FOCUSDOWN  && o->y+o->h > c->y + c->h
					&& INTERSECT(c->x, c->y, c->sw, c->sh+vague, o->x, o->y, o->sw, o->sh)))
			{
				client_activate(o, RAISEDEF, WARPDEF);
				return;
			}
		}
	}
	// we didn't find a window immediately adjacent or overlapping. look further afield
	winlist_descend(windows_in_play(c->xattr.root), i, w)
	{
		if ((o = window_client(w)) && o && o->manage && o->visible)
		{
			client_extended_data(o);
			if ((direction == FOCUSLEFT  && o->x < c->x) ||
				(direction == FOCUSRIGHT && o->x+o->w > c->x+c->w) ||
				(direction == FOCUSUP    && o->y < c->y) ||
				(direction == FOCUSDOWN  && o->y+o->h > c->y + c->h))
			{
				client_activate(o, RAISEDEF, WARPDEF);
				return;
			}
		}
	}
}

// search and activate first open window matching class/name/title
void app_find_or_start(Window root, char *pattern)
{
	if (!pattern) return;

	Window w, found = None; client *c = NULL;
	int i, j, offsets[] = { offsetof(client, name), offsetof(client, class), offsetof(client, title) };
	// first, try in current_tag only
	for (i = 0; i < 3 && found == None; i++)
	{
		winlist_descend(windows_activated, j, w)
		{
			if ((c = window_client(w)) && c && c->manage && c->visible && c->cache->tags & current_tag)
			{
				client_descriptive_data(c);
				if (!strcasecmp(((char*)c)+offsets[i], pattern))
					{ found = w; break; }
			}
		}
	}
	// failing that, search regardless of tag
	for (i = 0; i < 3 && found == None; i++)
	{
		winlist_descend(windows_activated, j, w)
		{
			if ((c = window_client(w)) && c && c->manage && c->visible)
			{
				client_descriptive_data(c);
				if (!strcasecmp(((char*)c)+offsets[i], pattern))
					{ found = w; break; }
			}
		}
	}
	if (found != None)
		client_activate(c, RAISE, WARPDEF);
	else exec_cmd(pattern);
}

// built-in filterable popup menu list
struct localmenu {
	Window window;
	GC gc;
	Pixmap canvas;
	XftFont *font;
	XftColor *color;
	XftDraw *draw;
	XftColor fg, bg, hlfg, hlbg;
	unsigned long xbg;
	char **lines, **filtered;
	int done, max_lines, num_lines, input_size, line_height;
	int current, width, height, horz_pad, vert_pad, offset;
	char *input, *selected, *manual;
	XIM xim;
	XIC xic;
};

// redraw the popup menu window
void menu_draw(struct localmenu *my)
{
	int i, n;

	// draw text input bar
	XftDrawRect(my->draw, &my->bg, 0, 0, my->width, my->height);
	XftDrawStringUtf8(my->draw, &my->fg, my->font, my->horz_pad, my->vert_pad+my->line_height-my->font->descent, (unsigned char*)my->input, strlen(my->input));

	// filter lines by current input text
	memset(my->filtered, 0, sizeof(char*) * (my->num_lines+1));
	for (i = 0, n = 0; my->lines[i]; i++)
	{
		if (!my->offset || strcasestr(my->lines[i], my->input))
			my->filtered[n++] = my->lines[i];
	}
	// vertical bounds of highlight bar
	my->current = MAX(0, MIN(my->current, n-1));
	for (i = 0; my->filtered[i]; i++)
	{
		XftColor fg = my->fg;
		// vertical position of *top* of current line
		int y = my->vert_pad+(my->line_height*(i+1));
		// http://en.wikipedia.org/wiki/Typeface#Font_metrics
		int font_baseline = y + my->line_height - my->font->descent -1;
		// are we highlighting this line?
		if (i == my->current)
		{
			fg = my->hlfg;
			XftDrawRect(my->draw, &my->hlbg, my->horz_pad, y, my->width-(my->horz_pad*2), my->line_height);
		}
		XftDrawStringUtf8(my->draw, &fg, my->font, my->horz_pad, font_baseline, (unsigned char*)my->filtered[i], strlen(my->filtered[i]));
	}
	// double buffering
	XCopyArea(display, my->canvas, my->window, my->gc, 0, 0, my->width, my->height, 0, 0);
}

// handle popup menu text input for filtering
void menu_key(struct localmenu *my, XEvent *ev)
{
	char pad[32]; KeySym key; Status stat;
	int len = XmbLookupString(my->xic, &ev->xkey, pad, sizeof(pad), &key, &stat);
	if (stat == XBufferOverflow) return;
	pad[len] = 0;

	key = XkbKeycodeToKeysym(display, ev->xkey.keycode, 0, 0);

	if (key == XK_Escape)
		my->done = 1;
	else
	if (key == XK_BackSpace)
	{
		if (my->offset > 0)
			my->input[--(my->offset)] = 0;
	}
	else
	if (key == XK_Up)
		my->current = (my->current == 0 ? my->max_lines-1: my->current-1);
	else
	if (key == XK_Down || key == XK_Tab)
		my->current = (my->current == my->max_lines-1 ? 0: my->current+1);
	else
	if (key == XK_Return)
	{
		if (my->filtered[my->current])
			my->selected = my->filtered[my->current];
		else
		if (my->manual)
			strcpy(my->manual, my->input);
		my->done = 1;
	}
	else
	if (!iscntrl(*pad) && my->offset < my->input_size-1)
	{
		my->input[my->offset++] = *pad;
		my->input[my->offset] = 0;
	}
	menu_draw(my);
}

// take over keyboard for popup menu
int menu_grab(struct localmenu *my)
{
	int i;
	for (i = 0; i < 1000; i++)
	{
		if (XGrabKeyboard(display, my->window, True, GrabModeAsync, GrabModeAsync, CurrentTime) == GrabSuccess)
			return 1;
		usleep(1000);
	}
	return 0;
}

// menu
int menu(Window root, char **lines, char *manual)
{
	int i, l, scr;
	struct localmenu _my, *my = &_my;

	XWindowAttributes *attr = window_get_attributes(root);
	workarea mon; monitor_active(attr->screen, &mon);
	scr = XScreenNumberOfScreen(attr->screen);

	// this never fails, afaics. we get some sort of font, no matter what
	my->font = XftFontOpenName(display, scr, config_menu_font);
	XftColorAllocName(display, DefaultVisual(display, scr), DefaultColormap(display, scr), config_menu_fg, &my->fg);
	XftColorAllocName(display, DefaultVisual(display, scr), DefaultColormap(display, scr), config_menu_bg, &my->bg);
	XftColorAllocName(display, DefaultVisual(display, scr), DefaultColormap(display, scr), config_menu_hlfg, &my->hlfg);
	XftColorAllocName(display, DefaultVisual(display, scr), DefaultColormap(display, scr), config_menu_hlbg, &my->hlbg);
	my->line_height = my->font->ascent + my->font->descent +2; // +2 pixel extra line spacing

	for (l = 0, i = 0; lines[i]; i++) l = MAX(l, strlen(lines[i]));

	my->lines       = lines;
	my->num_lines   = i;
	my->max_lines   = MIN(config_menu_lines, my->num_lines);
	my->input_size  = MAX(l, 100);
	my->filtered    = allocate_clear(sizeof(char*) * (my->num_lines+1));
	my->input       = allocate_clear((my->input_size+1)*3); // utf8 in copied line
	my->current     = 1; // index of currently highlighted line
	my->offset      = 0; // length of text in input buffer
	my->done        = 0; // bailout flag
	my->horz_pad    = 5; // horizontal padding
	my->vert_pad    = 5; // vertical padding
	my->width       = (mon.w/100)*config_menu_width;
	my->height      = ((my->line_height) * (my->max_lines+1)) + (my->vert_pad*2);
	my->xbg         = color_get(display, config_menu_bg);
	my->selected    = NULL;
	my->manual      = manual;

	int x = mon.x + ((mon.w - my->width)/2);
	int y = mon.y + (mon.h/2) - (my->height/2);

	my->window = XCreateSimpleWindow(display, root, x, y, my->width, my->height, 0, my->xbg, my->xbg);
	// make it an unmanaged window
	window_set_atom_prop(my->window, netatoms[_NET_WM_STATE], &netatoms[_NET_WM_STATE_ABOVE], 1);
	window_set_atom_prop(my->window, netatoms[_NET_WM_WINDOW_TYPE], &netatoms[_NET_WM_WINDOW_TYPE_DOCK], 1);
	XSelectInput(display, my->window, ExposureMask|KeyPressMask);

	// drawing environment
	my->gc     = XCreateGC(display, my->window, 0, 0);
	my->canvas = XCreatePixmap(display, root, my->width, my->height, DefaultDepth(display, scr));
	my->draw   = XftDrawCreate(display, my->canvas, DefaultVisual(display, scr), DefaultColormap(display, scr));

	// input keymap->charmap handling
	my->xim = XOpenIM(display, NULL, NULL, NULL);
	my->xic = XCreateIC(my->xim, XNInputStyle, XIMPreeditNothing | XIMStatusNothing, XNClientWindow, my->window, XNFocusWindow, my->window, NULL);

	menu_draw(my);
	XMapRaised(display, my->window);
	if (!menu_grab(my))
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
	}
	free(my->filtered);
	XftDrawDestroy(my->draw);
	XFreeGC(display, my->gc);
	XftFontClose(display, my->font);
	XUngrabKeyboard(display, CurrentTime);
	free(my->input);

	if (my->selected)
		for (i = 0; my->lines[i]; i++)
			if (my->lines[i] == my->selected)
				return i;
	return -1;
}

// built-in window switcher
void window_switcher(Window root, unsigned int tag)
{
	// TODO: this whole function is messy. build a nicer solution
	char pattern[50], **list = NULL;
	int i, classfield = 0, maxtags = 0, lines = 0, above = 0, sticky = 0, plen = 0;
	Window w; client *c; winlist *ids = winlist_new();

	// calc widths of wm_class and tag csv fields
	winlist_descend(windows_activated, i, w)
	{
		if ((c = window_client(w)) && c->manage && c->visible && !client_has_state(c, netatoms[_NET_WM_STATE_SKIP_TASKBAR]))
		{
			client_descriptive_data(c);
			if (!tag || (c->cache && c->cache->tags & tag))
			{
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
	maxtags = MAX(0, (maxtags*2)-1);
	if (above || sticky) plen = sprintf(pattern, "%%-%ds  ", above+sticky);
	if (maxtags) plen += sprintf(pattern+plen, "%%-%ds  ", maxtags);
	plen += sprintf(pattern+plen, "%%-%ds   %%s", MAX(5, classfield));
	list = allocate_clear(sizeof(char*) * (lines+1)); lines = 0;
	// build the actual list
	winlist_ascend(ids, i, w)
	{
		if ((c = window_client(w)))
		{
			client_descriptive_data(c);
			if (!tag || (c->cache && c->cache->tags & tag))
			{
				char tags[32]; memset(tags, 0, 32);
				int j, l; for (l = 0, j = 0; j < TAGS; j++)
					if (c->cache->tags & (1<<j)) l += sprintf(tags+l, "%d,", j+1);
				if (l > 0) tags[l-1] = '\0';

				char aos[5]; memset(aos, 0, 5);
				if (client_has_state(c, netatoms[_NET_WM_STATE_ABOVE])) strcat(aos, "a");
				if (client_has_state(c, netatoms[_NET_WM_STATE_STICKY])) strcat(aos, "s");

				char *line = allocate(strlen(c->title) + strlen(tags) + strlen(c->class) + classfield + 50);
				if ((above || sticky) && maxtags) sprintf(line, pattern, aos, tags, c->class, c->title);
				else if (maxtags) sprintf(line, pattern, tags, c->class, c->title);
				else sprintf(line, pattern, c->class, c->title);
				list[lines++] = line;
			}
		}
	}
	if (!fork())
	{
		display = XOpenDisplay(0);
		XSync(display, True);
		int n = menu(root, list, NULL);
		if (n >= 0 && list[n])
		{
			window_send_message(root, ids->array[n], netatoms[_NET_ACTIVE_WINDOW], 2, // 2 = pager
				SubstructureNotifyMask | SubstructureRedirectMask);
		}
		exit(EXIT_SUCCESS);
	}
	for (i = 0; i < lines; i++) free(list[i]);
	free(list); winlist_free(ids);
}

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
#ifndef DEBUG
	if (key == keymap[KEY_DEBUG]) return;
#endif
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
		for (i = 0; keymap[i]; i++) grab_key(root, keymap[i]);
		for (i = 0; config_apps_keysyms[i]; i++) if (config_apps_patterns[i]) grab_key(root, config_apps_keysyms[i]);
		for (i = 0; config_tags_keysyms[i]; i++) grab_key(root, config_tags_keysyms[i]);
		// grab mouse buttons for click-to-focus. these get passed through to the windows
		// not binding on button4 which is usually wheel scroll
		XUngrabButton(display, AnyButton, AnyModifier, root);
		XGrabButton(display, Button1, AnyModifier, root, True, ButtonPressMask, GrabModeSync, GrabModeSync, None, None);
		XGrabButton(display, Button2, AnyModifier, root, True, ButtonPressMask, GrabModeSync, GrabModeSync, None, None);
		XGrabButton(display, Button3, AnyModifier, root, True, ButtonPressMask, GrabModeSync, GrabModeSync, None, None);
	}
}

// MODKEY+keys
void handle_keypress(XEvent *ev)
{
	event_log("KeyPress", ev->xany.window);
	KeySym key = XkbKeycodeToKeysym(display, ev->xkey.keycode, 0, 0);

	int i; client *c = NULL;

	if (key == keymap[KEY_SWITCH])
	{
		if (config_switcher) exec_cmd(config_switcher);
		else window_switcher(ev->xany.window, 0);
	}
	else if (key == keymap[KEY_LAUNCH]) exec_cmd(config_launcher);

	// custom MODKEY launchers
	// on the command line: goomwwm -1 "firefox"
	else if ((i = in_array_keysym(config_apps_keysyms, key)) >= 0)
		app_find_or_start(ev->xany.window, config_apps_patterns[i]);

	else if ((i = in_array_keysym(config_tags_keysyms, key)) >= 0)
		tag_raise(1<<i);

	// tag cycling
	else if (key == keymap[KEY_TAGNEXT]) tag_raise(current_tag & TAG9 ? TAG1: current_tag<<1);
	else if (key == keymap[KEY_TAGPREV]) tag_raise(current_tag & TAG1 ? TAG9: current_tag>>1);

	else
	// following only relevant with a focused window
	if ((c = window_active_client(ev->xany.window, 0)) && c)
	{
		client_descriptive_data(c);
		client_extended_data(c);

		int screen_x = c->monitor.x, screen_y = c->monitor.y;
		int screen_width = c->monitor.w, screen_height = c->monitor.h;
		int vague = screen_width/100;

		// window co-ords translated to 0-based on screen
		int x = c->sx; int y = c->sy; int w = c->sw; int h = c->sh;

		// four basic window sizes
		int width1  = screen_width/3;      int height1 = screen_height/3;
		int width2  = screen_width/2;      int height2 = screen_height/2;
		int width3  = screen_width-width1; int height3 = screen_height-height1;
		int width4  = screen_width;        int height4 = screen_height;

		// final resize/move params. smart = intelligently bump / center / shrink
		int fx = 0, fy = 0, fw = 0, fh = 0, smart = 0;

		if (key == keymap[KEY_CLOSE]) client_close(c);
#ifdef DEBUG
		else if (key == keymap[KEY_DEBUG]) event_client_dump(c);
#endif
		else if (key == keymap[KEY_CYCLE]) client_cycle(c);
		else if (key == keymap[KEY_TAG]) client_toggle_tag(c, current_tag, FLASH);
		else if (key == keymap[KEY_ABOVE]) client_nws_above(c, TOGGLE);
		else if (key == keymap[KEY_BELOW]) client_nws_below(c, TOGGLE);
		else if (key == keymap[KEY_STICKY]) client_nws_sticky(c, TOGGLE);
		else if (key == keymap[KEY_FULLSCREEN]) client_nws_fullscreen(c, TOGGLE);
		else if (key == keymap[KEY_HMAX]) client_nws_maxhorz(c, TOGGLE);
		else if (key == keymap[KEY_VMAX]) client_nws_maxvert(c, TOGGLE);
		else if (key == keymap[KEY_EXPAND]) client_expand(c, HORIZONTAL|VERTICAL, 0, 0, 0, 0, 0, 0, 0, 0);
		else if (key == keymap[KEY_CONTRACT]) client_contract(c, HORIZONTAL|VERTICAL);
		else if (key == keymap[KEY_EHMAX]) client_expand(c, HORIZONTAL, 0, 0, 0, 0, 0, 0, 0, 0);
		else if (key == keymap[KEY_EVMAX]) client_expand(c, VERTICAL, 0, 0, 0, 0, 0, 0, 0, 0);
		else if (key == keymap[KEY_HTILE]) client_htile(c);
		else if (key == keymap[KEY_VTILE]) client_vtile(c);
		else if (key == keymap[KEY_UNDO]) client_rollback(c);

		// directional focus change
		else if (key == keymap[KEY_FOCUSLEFT]) client_focusto(c, FOCUSLEFT);
		else if (key == keymap[KEY_FOCUSRIGHT]) client_focusto(c, FOCUSRIGHT);
		else if (key == keymap[KEY_FOCUSUP]) client_focusto(c, FOCUSUP);
		else if (key == keymap[KEY_FOCUSDOWN]) client_focusto(c, FOCUSDOWN);

		else
		// cycle through windows with same tag
		if (key == keymap[KEY_TSWITCH])
			window_switcher(c->xattr.root, current_tag);
		else
		if (key == keymap[KEY_GROW] || key == keymap[KEY_SHRINK])
		{
			// a maxh/maxv client stays that way
			if (client_has_state(c, netatoms[_NET_WM_STATE_MAXIMIZED_HORZ]))
				width1 = width2 = width3 = width4 = screen_width;

			if (client_has_state(c, netatoms[_NET_WM_STATE_MAXIMIZED_VERT]))
				height1 = height2 = height3 = height4 = screen_height;

			smart = 1;
			// window width zone
			int isw4 = w >= width4 ?1:0;
			int isw3 = !isw4 && w >= width3 ?1:0;
			int isw2 = !isw4 && !isw3 && w >= width2 ?1:0;
			int isw1 = !isw4 && !isw3 && !isw2 && w >= width1 ?1:0;

			// window height zone
			int ish4 = h >= height4 ?1:0;
			int ish3 = !ish4 && h >= height3 ?1:0;
			int ish2 = !ish4 && !ish3 && h >= height2 ?1:0;
			int ish1 = !ish4 && !ish3 && !ish2 && h >= height1 ?1:0;

			// window zone ballpark. try to make resize changes intuitive base on window area
			int is4 = (isw4 && ish4) || (w*h >= width4*height4) ?1:0;
			int is3 = !is4 && ((isw3 && ish3) || (w*h >= width3*height3)) ?1:0;
			int is2 = !is4 && !is3 && ((isw2 && ish2) || (w*h >= width2*height2)) ?1:0;
			int is1 = !is4 && !is3 && !is2 && ((isw1 && ish1) || (w*h >= width1*height1)) ?1:0;
			int is0 = !is4 && !is3 && !is2 && !is1 ?1:0;

			// Page Up/Down makes the focused window larger and smaller respectively
			if (key == keymap[KEY_GROW])
			{
				fx = screen_x + c->sx; fy = screen_y + c->sy;
				if (is0) { fw = width1; fh = height1; }
				else if (is1) { fw = width2; fh = height2; }
				else if (is2) { fw = width3; fh = height3; }
				else if (is3) { fw = width4; fh = height4; }
				else { fw = width4; fh = height4; }
				smart = 1;
			}
			else
			if (key == keymap[KEY_SHRINK])
			{
				fx = screen_x + c->sx; fy = screen_y + c->sy;
				if (is4) { fw = width3; fh = height3; }
				else if (is3) { fw = width2; fh = height2; }
				else { fw = width1; fh = height1; }
				smart = 1;
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
				if (mon.x < c->monitor.x) { fx = mon.x+mon.w-w; fy = y; fw = w; fh = h; }
			}
			else
			if (key == keymap[KEY_RIGHT] && c->is_right)
			{
				monitor_dimensions_struts(c->xattr.screen, c->monitor.x+c->monitor.w+c->monitor.r+vague, c->y, &mon);
				if (mon.x > c->monitor.x) { fx = mon.x; fy = y; fw = w; fh = h; }
			}
			else
			if (key == keymap[KEY_UP] && c->is_top)
			{
				monitor_dimensions_struts(c->xattr.screen, c->x, c->monitor.y-c->monitor.t-vague, &mon);
				if (mon.y < c->monitor.y) { fx = x; fy = mon.y+mon.h-h; fw = w; fh = h; }
			}
			else
			if (key == keymap[KEY_DOWN] && c->is_bottom)
			{
				monitor_dimensions_struts(c->xattr.screen, c->x, c->monitor.y+c->monitor.h+c->monitor.b+vague, &mon);
				if (mon.y > c->monitor.y) { fx = x; fy = mon.y; fw = w; fh = h; }
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

// MODKEY+keys
void handle_keyrelease(XEvent *ev)
{
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

	if (ev->xbutton.subwindow != None && (c = window_client(ev->xbutton.subwindow)) && c && c->manage)
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
	int is_mod = state & config_modkey && !(state & config_ignore_modkeys);

	if (ev->xbutton.window != None && (c = window_client(ev->xbutton.window)) && c && c->manage)
	{
		int xd = ev->xbutton.x_root - mouse_button.x_root;
		int yd = ev->xbutton.y_root - mouse_button.y_root;

		// if no resize or move has occurred, allow Mod+Button3 to lower a window
		if (!xd && !yd && is_mod && ev->xbutton.button == Button3)
			client_lower(c, 0);
	}
	XUngrabPointer(display, CurrentTime);
}

// only get these if a window move/resize has been started in buttonpress
void handle_motionnotify(XEvent *ev)
{
	// compress events to reduce window jitter and CPU load
	while(XCheckTypedEvent(display, MotionNotify, ev));
	client *c = window_client(ev->xmotion.window);
	if (c && c->manage)
	{
		client_extended_data(c);
		int xd = ev->xbutton.x_root - mouse_button.x_root;
		int yd = ev->xbutton.y_root - mouse_button.y_root;
		int x  = mouse_attr.x + (mouse_button.button == Button1 ? xd : 0);
		int y  = mouse_attr.y + (mouse_button.button == Button1 ? yd : 0);
		int w  = MAX(1, mouse_attr.width  + (mouse_button.button == Button3 ? xd : 0));
		int h  = MAX(1, mouse_attr.height + (mouse_button.button == Button3 ? yd : 0));
		int vague = c->monitor.w/100;

		// snap to monitor edges
		// Button1 = move
		if (mouse_button.button == Button1)
		{
			if (NEAR(c->monitor.x, vague, x)) x = c->monitor.x;
			if (NEAR(c->monitor.y, vague, y)) y = c->monitor.y;
			if (NEAR(c->monitor.x+c->monitor.w, vague, x+w)) x = c->monitor.x+c->monitor.w-w-(config_border_width*2);
			if (NEAR(c->monitor.y+c->monitor.h, vague, y+h)) y = c->monitor.y+c->monitor.h-h-(config_border_width*2);
		}
		else
		// Button3 = resize
		if (mouse_button.button == Button3)
		{
			if (NEAR(c->monitor.x+c->monitor.w, vague, x+w)) w = c->monitor.x+c->monitor.w-x-(config_border_width*2);
			if (NEAR(c->monitor.y+c->monitor.h, vague, y+h)) h = c->monitor.y+c->monitor.h-y-(config_border_width*2);
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
			double minr = (double) c->xsize.min_aspect.x / c->xsize.min_aspect.y;
			double maxr = (double) c->xsize.max_aspect.x / c->xsize.max_aspect.y;
				if (ratio < minr) h = (int)(w / minr);
			else if (ratio > maxr) w = (int)(h * maxr);
		}
		XMoveResizeWindow(display, ev->xmotion.window, x, y, w, h);
		// cancel any cached moves done by client_moveresize()
		c->cache->have_mr = 0;

		// who knows where we've ended up. clear states
		client_remove_state(c, netatoms[_NET_WM_STATE_MAXIMIZED_HORZ]);
		client_remove_state(c, netatoms[_NET_WM_STATE_MAXIMIZED_VERT]);
	}
}

// we dont really care until a window configures and maps, so just watch it
void handle_createnotify(XEvent *ev)
{
	XSelectInput(display, ev->xcreatewindow.window, EnterWindowMask | LeaveWindowMask | FocusChangeMask | PropertyChangeMask);
	if (winlist_find(windows, ev->xcreatewindow.window) < 0)
	{
		wincache *cache = allocate_clear(sizeof(wincache));
		winlist_append(windows, ev->xcreatewindow.window, cache);
	}
	if (window_is_root(ev->xcreatewindow.parent)) ewmh_client_list(ev->xcreatewindow.parent);
}

// we don't track window state internally much, so this is just for info
void handle_destroynotify(XEvent *ev)
{
	Window w = ev->xdestroywindow.window;
	// remove any cached data on a window
	winlist_forget(windows, w);
	winlist_forget(windows_activated, w);
	if (window_is_root(ev->xdestroywindow.event))
		ewmh_client_list(ev->xdestroywindow.event);
}

// very loose with configure requests
// just let stuff go through mostly unchanged so apps can remember window positions/sizes
void handle_configurerequest(XEvent *ev)
{
	client *c = window_client(ev->xconfigurerequest.window);
	if (c)
	{
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

			// if we previously instructed the window to an x/y/w/h which conforms to
			// their w/h hints, demand co-operation!
			if (c->cache && c->cache->have_mr)
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
		ewmh_desktop_list(root);
		XWindowAttributes *attr = window_get_attributes(root);
		int i; Window w;
		// find all windows and ensure they're visible in the new screen layout
		winlist_ascend(windows_in_play(ev->xconfigure.window), i, w)
		{
			if ((c = window_client(w)) && c->manage && c->visible)
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
	}
	else
	if ((c = window_client(ev->xconfigure.window)) && c->manage)
	{
		event_log("ConfigureNotify", c->window);
		event_client_dump(c);
		client_review_border(c);
		client_review_position(c);
		if (c->active && config_warp_mode == WARPFOCUS)
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
	client *c = window_client(ev->xmaprequest.window);
	if (c && c->manage)
	{
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
			workarea *m = &c->monitor; workarea a;
			// try to center transients on their main window
			if (c->trans != None && (p = window_client(c->trans)) && p)
			{
				client_extended_data(p);
				m = &p->monitor;
			}
			else
			// center everything else on current monitor
			{
				monitor_active(c->xattr.screen, &a);
				m = &a;
			}
			client_moveresize(c, 0, MAX(m->x, m->x + ((m->w - c->w) / 2)),
				MAX(m->y, m->y + ((m->h - c->h) / 2)), c->w, c->h);
		}
		client_raise(c, 0);
		XSync(display, False);
	}
	XMapWindow(display, ev->xmaprequest.window);
}

// a newly mapped top-level window automatically takes focus
// this could be configurable?
void handle_mapnotify(XEvent *ev)
{
	client *c = window_client(ev->xmap.window);
	if (c && c->manage)
	{
		event_log("MapNotify", c->window);
		client_state(c, NormalState);
		if (!(c->cache->tags & current_tag))
			client_toggle_tag(c, current_tag, NOFLASH);
		client_activate(c, RAISEDEF, WARPDEF);
		ewmh_client_list(c->xattr.root);
	}
}

// unmapping could indicate the focus window has closed
// find a new one to focus if needed
void handle_unmapnotify(XEvent *ev)
{
	int was_active = window_is_active(ev->xunmap.window);
	client *c = window_client(ev->xunmap.window);
	// was it a top-level app window that closed?
	if (c && c->manage)
	{
		event_log("UnmapNotify", c->window);
		event_client_dump(c);
		client_state(c, WithdrawnState);
	}
	// if window has already been destroyed, above window_client() may have failed
	// see if this was the active window, and if so, find someone else to take the job
	if (was_active)
	{
		if (window_is_root(ev->xunmap.event))
		{
			window_active_client(ev->xunmap.event, current_tag);
			ewmh_client_list(ev->xunmap.event);
		}
		else
		if ((c = window_client(ev->xunmap.event)) && c && c->manage)
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
		client *c = window_client(m->window);
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
	client *c = window_client(p->window);
	if (c && c->visible && c->manage)
	{
		if (p->atom == atoms[WM_NAME] || p->atom == netatoms[_NET_WM_NAME])
			ewmh_client_list(c->xattr.root);
		if (p->atom == netatoms[_NET_WM_STATE_DEMANDS_ATTENTION] && !c->active)
			client_deactivate(c);
	}
}

// sloppy focus
void handle_enternotify(XEvent *ev)
{
	// only care about the sloppy modes here
	if (config_focus_mode == FOCUSCLICK) return;
	// ensure it's a proper enter event without keys or buttons down
	if (ev->xcrossing.type != EnterNotify) return;
	// prevent focus flicker if mouse is moving through multiple windows fast
	while(XCheckTypedEvent(display, EnterNotify, ev));

	client *c = window_client(ev->xcrossing.window);
	// FOCUSSLOPPY = any manageable window
	// FOCUSSLOPPYTAG = any manageable window in current tag
	if (c && c->visible && c->manage && !c->active && (config_focus_mode == FOCUSSLOPPY ||
		(config_focus_mode == FOCUSSLOPPYTAG && c->cache->tags & current_tag)))
	{
		event_log("EnterNotify", c->window);
		client_activate(c, RAISEDEF, WARPDEF);
	}
}
void handle_leavenotify(XEvent *ev)
{

}

void handle_mappingnotify(XEvent *ev)
{
	event_log("MappingNotify", ev->xany.window);
	while(XCheckTypedEvent(display, MappingNotify, ev));
	grab_keys_and_buttons();
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
		client *c = window_client(w);
		if (c && c->manage)
		{
			winlist_append(windows_activated, c->window, NULL);
			client_full_review(c);
		}
	}
	// activate and focus top window
	window_active_client(root, 0);
	ewmh_client_list(root);
	ewmh_desktop_list(root);
}

int main(int argc, char *argv[])
{
	int i, j, scr; XEvent ev;

	// catch help request
	if (find_arg(argc, argv, "-help") >= 0
		|| find_arg(argc, argv, "--help") >= 0
		|| find_arg(argc, argv, "-h") >= 0)
	{
		fprintf(stderr, "See the man page or visit http://github.com/seanpringle/goomwwm\n");
		return EXIT_FAILURE;
	}
	if(!(display = XOpenDisplay(0)))
	{
		fprintf(stderr, "cannot open display!\n");
		return EXIT_FAILURE;
	}
	signal(SIGCHLD, catch_exit);

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
#ifdef DEBUG
	for (i = 0; i < ac; i++) printf("%s\n", av[i]);
#endif

	// caches to reduce X server round trips during a single event
	cache_client = winlist_new();
	cache_xattr  = winlist_new();
	cache_inplay = winlist_new();

	// do this before setting error handler, so it fails if other wm in place
	XSelectInput(display, DefaultRootWindow(display), SubstructureRedirectMask);
	XSync(display, False); xerror = XSetErrorHandler(oops); XSync(display, False);

	// determine numlock mask so we can bind on keys with and without it
	XModifierKeymap *modmap = XGetModifierMapping(display);
	for (i = 0; i < 8; i++)
		for (j = 0; j < (int)modmap->max_keypermod; j++)
			if (modmap->modifiermap[i*modmap->max_keypermod+j] == XKeysymToKeycode(display, XK_Num_Lock))
				NumlockMask = (1<<i);
	XFreeModifiermap(modmap);

	// determine modkey
	config_modkey = MODKEY;
	char *modkeys = find_arg_str(ac, av, "-modkey", NULL);
	if (modkeys)
	{
		config_modkey = 0;
		if (strcasestr(modkeys, "shift"))   config_modkey |= ShiftMask;
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

	// custom keys
	for (i = 0; keyargs[i]; i++)
	{
		char *key = find_arg_str(ac, av, keyargs[i], NULL);
		if (key) keymap[i] = XStringToKeysym(key);
	}

	// border colors
	config_border_focus     = color_get(display, find_arg_str(ac, av, "-focus", FOCUS));
	config_border_blur      = color_get(display, find_arg_str(ac, av, "-blur",  BLUR));
	config_border_attention = color_get(display, find_arg_str(ac, av, "-attention", ATTENTION));

	// border width in pixels
	config_border_width = MAX(0, find_arg_int(ac, av, "-border", BORDER));

	// window flashing
	config_flash_on  = color_get(display, find_arg_str(ac, av, "-flashon",  FLASHON));
	config_flash_off = color_get(display, find_arg_str(ac, av, "-flashoff", FLASHOFF));
	config_flash_width = MAX(0, find_arg_int(ac, av, "-flashpx", FLASHPX));
	config_flash_ms    = MAX(FLASHMS, find_arg_int(ac, av, "-flashms", FLASHMS));

	// customizable keys
	config_switcher = find_arg_str(ac, av, "-switcher", SWITCHER);
	config_launcher = find_arg_str(ac, av, "-launcher", LAUNCHER);

	// window switcher
	config_menu_width = find_arg_int(ac, av, "-menuwidth", MENUWIDTH);
	config_menu_lines = find_arg_int(ac, av, "-menulines", MENULINES);
	config_menu_font  = find_arg_str(ac, av, "-menufont", MENUXFTFONT);
	config_menu_fg    = find_arg_str(ac, av, "-menufg", MENUFG);
	config_menu_bg    = find_arg_str(ac, av, "-menubg", MENUBG);
	config_menu_hlfg  = find_arg_str(ac, av, "-menuhlfg", MENUHLFG);
	config_menu_hlbg  = find_arg_str(ac, av, "-menuhlbg", MENUHLBG);

	char *mode;

	// focus mode
	config_focus_mode = FOCUSCLICK;
	mode = find_arg_str(ac, av, "-focusmode", "click");
	if (!strcasecmp(mode, "sloppy")) config_focus_mode = FOCUSSLOPPY;
	if (!strcasecmp(mode, "sloppytag")) config_focus_mode = FOCUSSLOPPYTAG;

	// raise mode
	config_raise_mode = RAISEFOCUS;
	mode = find_arg_str(ac, av, "-raisemode", "focus");
	if (!strcasecmp(mode, "click")) config_raise_mode = RAISECLICK;

	// warp mode
	config_warp_mode = WARPNEVER;
	mode = find_arg_str(ac, av, "-warpmode", "never");
	if (!strcasecmp(mode, "focus")) config_warp_mode = WARPFOCUS;

	// new-window placement mode
	config_window_placement = PLACEANY;
	mode = find_arg_str(ac, av, "-placement", "any");
	if (!strcasecmp(mode, "center"))  config_window_placement = PLACECENTER;
	if (!strcasecmp(mode, "pointer")) config_window_placement = PLACEPOINTER;

	// app_find_or_start() keys
	for (i = 0; config_apps_keysyms[i]; i++)
	{
		char tmp[3]; sprintf(tmp, "-%d", i);
		config_apps_patterns[i] = find_arg_str(ac, av, tmp, NULL);
	}
	// X atom values
	for (i = 0; i < ATOMS; i++) atoms[i] = XInternAtom(display, atom_names[i], False);
	for (i = 0; i < NETATOMS; i++) netatoms[i] = XInternAtom(display, netatom_names[i], False);

	// window tracking
	windows = winlist_new();
	windows_activated = winlist_new();

	// init on all screens/roots
	for (scr = 0; scr < ScreenCount(display); scr++) setup_screen(scr);
	grab_keys_and_buttons();

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

		if (ev.type == KeyPress) handle_keypress(&ev);
		else if (ev.type == KeyRelease) handle_keyrelease(&ev);
		else if (ev.type == ButtonPress) handle_buttonpress(&ev);
		else if (ev.type == ButtonRelease) handle_buttonrelease(&ev);
		else if (ev.type == MotionNotify) handle_motionnotify(&ev);
		else if (ev.type == CreateNotify) handle_createnotify(&ev);
		else if (ev.type == DestroyNotify) handle_destroynotify(&ev);
		else if (ev.type == ConfigureRequest) handle_configurerequest(&ev);
		else if (ev.type == ConfigureNotify) handle_configurenotify(&ev);
		else if (ev.type == MapRequest) handle_maprequest(&ev);
		else if (ev.type == MapNotify) handle_mapnotify(&ev);
		else if (ev.type == UnmapNotify) handle_unmapnotify(&ev);
		else if (ev.type == ClientMessage) handle_clientmessage(&ev);
		else if (ev.type == PropertyNotify) handle_propertynotify(&ev);
		else if (ev.type == EnterNotify) handle_enternotify(&ev);
		else if (ev.type == LeaveNotify) handle_leavenotify(&ev);
#ifdef DEBUG
		else fprintf(stderr, "unhandled event %d: %x\n", ev.type, (unsigned int)ev.xany.window);
		catch_exit(0);
#endif
	}
	return EXIT_SUCCESS;
}
