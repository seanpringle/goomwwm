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

void catch_exit(int sig)
{
	while (0 < waitpid(-1, NULL, WNOHANG));
}
// execute sub-process and connect its stdin=infp and stdout=outfp
pid_t exec_cmd_io(const char *command, int *infp, int *outfp)
{
	signal(SIGCHLD, catch_exit);

	int p_stdin[2], p_stdout[2];
	if (pipe(p_stdin) != 0 || pipe(p_stdout) != 0) return -1;

	pid_t pid = fork();
	// child process
	if (!pid)
	{
		close(p_stdin[WRITE]); dup2(p_stdin[READ], READ);
		close(p_stdout[READ]); dup2(p_stdout[WRITE], WRITE);
		execlp("/bin/sh", "sh", "-c", command, NULL);
		exit(EXIT_FAILURE); // should never get here!
	}
	else
	// error in fork
	if (pid < 0)
	{
		close(p_stdin[READ]);  close(p_stdin[WRITE]);
		close(p_stdout[READ]); close(p_stdout[WRITE]);
	}
	else
	// all good!
	{
		if (infp  == NULL) close(p_stdin[WRITE]); else *infp  = p_stdin[WRITE];
		if (outfp == NULL) close(p_stdout[READ]); else *outfp = p_stdout[READ];
		close(p_stdin[READ]); close(p_stdout[WRITE]);
	}
	return pid;
}
// execute sub-process
pid_t exec_cmd(char *cmd)
{
	if (!cmd || !cmd[0]) return -1;
	pid_t pid;
	signal(SIGCHLD, catch_exit);
	pid = fork();
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
	int i = 0;
	while (i < argc && strcasecmp(argv[i], key) != 0) i++;
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
int find_arg_flag(int argc, char *argv[], char *key, int def)
{
	char *opts[] = { "on", "yes", "off", "no", "1", "0" };
	int opt = find_arg_opts(argc, argv, key, opts, 6);
	if (opt == 0 || opt == 2 || opt == 4) return 1;
	if (opt == 1 || opt == 3 || opt == 5) return 0;
	return def;
}

#define CLIENTTITLE 100
#define CLIENTCLASS 50
#define CLIENTNAME 50
#define CLIENTROLE 50
#define CLIENTSTATE 10

// for filtering XQueryTree results
typedef struct {
	Window parent;
	int position;
	char class[50];
} search;

// window lists
typedef struct {
	Window *array;
	void **data;
	int len;
} winlist;

#define TOPLEFT 1
#define TOPRIGHT 2
#define BOTTOMLEFT 3
#define BOTTOMRIGHT 4

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
	char title[CLIENTTITLE], class[CLIENTCLASS], name[CLIENTNAME], role[CLIENTROLE];
	Atom state[CLIENTSTATE], type;
	workarea monitor;
	wincache *cache;
} client;

#define BORDER 2
#define FOCUS "Royal Blue"
#define BLUR "Dark Gray"
#define FLASHON "Dark Green"
#define FLASHOFF "Dark Red"
#define SWITCHER NULL
#define SWITCHER_BUILTIN "dmenu -i -l 25 -wp 60 -hc -vc -fn -*-terminus-medium-r-*-*-24-*-*-*-*-*-*-*"
#define LAUNCHER "dmenu_run"
#define FLASHPX 20
#define FLASHMS 300
#define MODKEY Mod4Mask

unsigned int config_modkey, config_ignore_modkeys,
	config_border_focus,  config_border_blur,
	config_flash_on, config_flash_off,
	config_border_width, config_flash_width, config_flash_ms;

char *config_switcher, *config_launcher,
	*config_key_1,  *config_key_2,  *config_key_3,  *config_key_4,   *config_key_5,
	*config_key_6,  *config_key_7,  *config_key_8,  *config_key_9;

unsigned int NumlockMask = 0;
Display *display;
XButtonEvent mouse_button;
XWindowAttributes mouse_attr;
winlist *windows, *windows_activated;
Window supporting;
unsigned int currenttag = TAG1;

static int (*xerror)(Display *, XErrorEvent *);

// atoms that we care about
enum {
	WM_PROTOCOLS,
	WM_DELETE_WINDOW,
	WM_STATE,
	WM_TAKE_FOCUS,
	WM_NAME,
	WM_CLASS,
	WM_WINDOW_ROLE,
	ATOMS
};
const char *atom_names[] = {
	[WM_PROTOCOLS] = "WM_PROTOCOLS",
	[WM_DELETE_WINDOW] = "WM_DELETE_WINDOW",
	[WM_STATE] = "WM_STATE",
	[WM_TAKE_FOCUS] = "WM_TAKE_FOCUS",
	[WM_NAME] = "WM_NAME",
	[WM_CLASS] = "WM_CLASS",
	[WM_WINDOW_ROLE] = "WM_WINDOW_ROLE",
};
enum {
	NET_SUPPORTED,
	NET_SUPPORTING_WM_CHECK,
	NET_WM_NAME,
	NET_WM_PID,
	NET_CLIENT_LIST,
	NET_CLIENT_LIST_STACKING,
	NET_NUMBER_OF_DESKTOPS,
	NET_CURRENT_DESKTOP,
	NET_DESKTOP_GEOMETRY,
	NET_DESKTOP_VIEWPORT,
	NET_WORKAREA,
	NET_ACTIVE_WINDOW,
	NET_CLOSE_WINDOW,
	NET_MOVERESIZE_WINDOW,
	NET_WM_WINDOW_TYPE,
	NET_WM_WINDOW_TYPE_DESKTOP,
	NET_WM_WINDOW_TYPE_DOCK,
	NET_WM_WINDOW_TYPE_SPLASH,
	NET_WM_WINDOW_TYPE_UTILITY,
	NET_WM_WINDOW_TYPE_TOOLBAR,
	NET_WM_WINDOW_TYPE_MENU,
	NET_WM_WINDOW_TYPE_DIALOG,
	NET_WM_WINDOW_TYPE_NORMAL,
	NET_WM_STATE,
	NET_WM_STATE_MODAL,
	NET_WM_STATE_STICKY,
	NET_WM_STATE_MAXIMIZED_VERT,
	NET_WM_STATE_MAXIMIZED_HORZ,
	NET_WM_STATE_SHADED,
	NET_WM_STATE_SKIP_TASKBAR,
	NET_WM_STATE_SKIP_PAGER,
	NET_WM_STATE_HIDDEN,
	NET_WM_STATE_FULLSCREEN,
	NET_WM_STATE_ABOVE,
	NET_WM_STATE_BELOW,
	NET_WM_STATE_DEMANDS_ATTENTION,
	NET_WM_STATE_ADD,
	NET_WM_STATE_REMOVE,
	NET_WM_STATE_TOGGLE,
	NET_WM_ALLOWED_ACTIONS,
	NET_WM_ACTION_MOVE,
	NET_WM_ACTION_RESIZE,
	NET_WM_ACTION_MINIMIZE,
	NET_WM_ACTION_SHADE,
	NET_WM_ACTION_STICK,
	NET_WM_ACTION_MAXIMIZE_VERT,
	NET_WM_ACTION_MAXIMIZE_HORZ,
	NET_WM_ACTION_FULLSCREEN,
	NET_WM_ACTION_CHANGE_DESKTOP,
	NET_WM_ACTION_CLOSE,
	NET_WM_STRUT,
	NET_WM_STRUT_PARTIAL,
	NETATOMS
};
const char *netatom_names[] = {
	[NET_SUPPORTED] = "_NET_SUPPORTED",
	[NET_SUPPORTING_WM_CHECK] = "_NET_SUPPORTING_WM_CHECK",
	[NET_WM_NAME] = "_NET_WM_NAME",
	[NET_WM_PID] = "_NET_WM_PID",
	[NET_CLIENT_LIST] = "_NET_CLIENT_LIST",
	[NET_CLIENT_LIST_STACKING] = "_NET_CLIENT_LIST_STACKING",
	[NET_NUMBER_OF_DESKTOPS] = "_NET_NUMBER_OF_DESKTOPS",
	[NET_CURRENT_DESKTOP] = "_NET_CURRENT_DESKTOP",
	[NET_DESKTOP_GEOMETRY] = "_NET_DESKTOP_GEOMETRY",
	[NET_DESKTOP_VIEWPORT] = "_NET_DESKTOP_VIEWPORT",
	[NET_WORKAREA] = "_NET_WORKAREA",
	[NET_ACTIVE_WINDOW] = "_NET_ACTIVE_WINDOW",
	[NET_CLOSE_WINDOW] = "_NET_CLOSE_WINDOW",
	[NET_MOVERESIZE_WINDOW] = "_NET_MOVERESIZE_WINDOW",
	[NET_WM_WINDOW_TYPE] = "_NET_WM_WINDOW_TYPE",
	[NET_WM_WINDOW_TYPE_DESKTOP] = "_NET_WM_WINDOW_TYPE_DESKTOP",
	[NET_WM_WINDOW_TYPE_DOCK] = "_NET_WM_WINDOW_TYPE_DOCK",
	[NET_WM_WINDOW_TYPE_SPLASH] = "_NET_WM_WINDOW_TYPE_SPLASH",
	[NET_WM_WINDOW_TYPE_UTILITY] = "_NET_WM_WINDOW_TYPE_UTILITY",
	[NET_WM_WINDOW_TYPE_TOOLBAR] = "_NET_WM_WINDOW_TYPE_TOOLBAR",
	[NET_WM_WINDOW_TYPE_MENU] = "_NET_WM_WINDOW_TYPE_MENU",
	[NET_WM_WINDOW_TYPE_DIALOG] = "_NET_WM_WINDOW_TYPE_DIALOG",
	[NET_WM_WINDOW_TYPE_NORMAL] = "_NET_WM_WINDOW_TYPE_NORMAL",
	[NET_WM_STATE] = "_NET_WM_STATE",
	[NET_WM_STATE_MODAL] = "_NET_WM_STATE_MODAL",
	[NET_WM_STATE_STICKY] = "_NET_WM_STATE_STICKY",
	[NET_WM_STATE_MAXIMIZED_VERT] = "_NET_WM_STATE_MAXIMIZED_VERT",
	[NET_WM_STATE_MAXIMIZED_HORZ] = "_NET_WM_STATE_MAXIMIZED_HORZ",
	[NET_WM_STATE_SHADED] = "_NET_WM_STATE_SHADED",
	[NET_WM_STATE_SKIP_TASKBAR] = "_NET_WM_STATE_SKIP_TASKBAR",
	[NET_WM_STATE_SKIP_PAGER] = "_NET_WM_STATE_SKIP_PAGER",
	[NET_WM_STATE_HIDDEN] = "_NET_WM_STATE_HIDDEN",
	[NET_WM_STATE_FULLSCREEN] = "_NET_WM_STATE_FULLSCREEN",
	[NET_WM_STATE_ABOVE] = "_NET_WM_STATE_ABOVE",
	[NET_WM_STATE_BELOW] = "_NET_WM_STATE_BELOW",
	[NET_WM_STATE_DEMANDS_ATTENTION] = "_NET_WM_STATE_DEMANDS_ATTENTION",
	[NET_WM_STATE_ADD] = "_NET_WM_STATE_ADD",
	[NET_WM_STATE_REMOVE] = "_NET_WM_STATE_REMOVE",
	[NET_WM_STATE_TOGGLE] = "_NET_WM_STATE_TOGGLE",
	[NET_WM_ALLOWED_ACTIONS] = "_NET_WM_ALLOWED_ACTIONS",
	[NET_WM_ACTION_MOVE] = "_NET_WM_ACTION_MOVE",
	[NET_WM_ACTION_RESIZE] = "_NET_WM_ACTION_RESIZE",
	[NET_WM_ACTION_MINIMIZE] = "_NET_WM_ACTION_MINIMIZE",
	[NET_WM_ACTION_SHADE] = "_NET_WM_ACTION_SHADE",
	[NET_WM_ACTION_STICK] = "_NET_WM_ACTION_STICK",
	[NET_WM_ACTION_MAXIMIZE_VERT] = "_NET_WM_ACTION_MAXIMIZE_VERT",
	[NET_WM_ACTION_MAXIMIZE_HORZ] = "_NET_WM_ACTION_MAXIMIZE_HORZ",
	[NET_WM_ACTION_FULLSCREEN] = "_NET_WM_ACTION_FULLSCREEN",
	[NET_WM_ACTION_CHANGE_DESKTOP] = "_NET_WM_ACTION_CHANGE_DESKTOP",
	[NET_WM_ACTION_CLOSE] = "_NET_WM_ACTION_CLOSE",
	[NET_WM_STRUT] = "_NET_WM_STRUT",
	[NET_WM_STRUT_PARTIAL] = "_NET_WM_STRUT_PARTIAL",
};
Atom atoms[ATOMS];
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

winlist* winlist_new()
{
	winlist *l = malloc(sizeof(winlist));
	l->array = malloc(sizeof(Window) * 9);
	l->data  = malloc(sizeof(void*) * 9);
	l->len = 0;
	return l;
}
typedef int (*winlist_cb)(int,Window,void*);
void winlist_iterate_up(winlist *l, winlist_cb cb, void *p)
{
	int i; for (i = 0; i < l->len; i++) if (cb(i, l->array[i], p)) break;
}
void winlist_iterate_down(winlist *l, winlist_cb cb, void *p)
{
	int i; for (i = l->len-1; i > -1; i--) if(cb(i, l->array[i], p)) break;
}
void winlist_append(winlist *l, Window w, void *d)
{
	if (l->len > 0 && !(l->len%8))
	{
		l->array = realloc(l->array, sizeof(Window) * (l->len+9));
		l->data  = realloc(l->data,  sizeof(void*)  * (l->len+9));
	}
	l->data[l->len] = d;
	l->array[l->len++] = w;
}
void winlist_free(winlist *l)
{
	int i; for (i = 0; i < l->len; i++)
		if (l->data[i]) free(l->data[i]);
	free(l->array); free(l->data); free(l);
}
int winlist_find(winlist *l, Window w)
{
	int i; for (i = 0; i < l->len; i++) if (l->array[i] == w) return i;
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

unsigned int XGetColor(Display *d, const char *name)
{
	XColor color;
	Colormap map = DefaultColormap(d, DefaultScreen(d));
	return XAllocNamedColor(d, map, name, &color, &color) ? color.pixel: None;
}

// check if a window id matches a known root window
int window_is_root(Window w)
{
	int scr; for (scr = 0; scr < ScreenCount(display); scr++)
		if (RootWindow(display, scr) == w) return 1;
	return 0;
}

char* window_get_text_prop(Window w, Atom atom)
{
	XTextProperty prop; char *res = NULL;
	char **list = NULL; int count;
	if (XGetTextProperty(display, w, &prop, atom) && prop.value && prop.nitems)
	{
		if (prop.encoding == XA_STRING)
		{
			res = malloc(strlen((char*)prop.value)+1);
			strcpy(res, (char*)prop.value);
		}
		else
		if (XmbTextPropertyToTextList(display, &prop, &list, &count) >= Success && count > 0 && *list)
		{
			res = malloc(strlen(*list)+1);
			strcpy(res, *list);
			XFreeStringList(list);
		}
	}
	if (prop.value) XFree(prop.value);
	return res;
}

Atom window_get_atom_prop(Window w, Atom atom)
{
	Atom prop = None, a; int b; unsigned long c = 0, d; unsigned char *res;
	if (XGetWindowProperty(display, w, atom, 0L, sizeof(Atom), False, XA_ATOM,
		&a, &b, &c, &d, &res) == Success && res)
	{
		if (a == XA_ATOM && c > 0)
			prop = *(Atom*)res;
		XFree(res);
	}
	return prop;
}

int window_get_atom_proplist(Window w, Atom atom, Atom *list, int max)
{
	Atom a; int b; unsigned long c = 0, d; unsigned char *res;
	if (XGetWindowProperty(display, w, atom, 0L, sizeof(Atom), False, XA_ATOM,
		&a, &b, &c, &d, &res) == Success && res)
	{
		if (a == XA_ATOM && c > 0)
			memmove(list, res, sizeof(Atom) * MIN(max, c));
		XFree(res);
	}
	return (int)c;
}

void window_set_atom_prop(Window w, Atom prop, Atom *atoms, int count)
{
	XChangeProperty(display, w, prop, XA_ATOM, 32, PropModeReplace, (unsigned char*)atoms, count);
}

void window_unset_prop(Window w, Atom prop)
{
	XDeleteProperty(display, w, prop);
}

int window_send_message(Window target, Window subject, Atom atom, unsigned long protocol, unsigned long mask)
{
    XEvent e; memset(&e, 0, sizeof(XEvent));
    e.xclient.type = ClientMessage;
    e.xclient.send_event = True;
    e.xclient.message_type = atom;
    e.xclient.window = subject;
    e.xclient.format = 32;
    e.xclient.data.l[0] = protocol;
    e.xclient.data.l[1] = CurrentTime;
	int r = XSendEvent(display, target, False, mask, &e) ?1:0;
	XFlush(display);
	return r;
}

// top-level, visible windows. DOES include non-managable docks/panels
winlist* windows_in_play(Window root)
{
	winlist *l = winlist_new();
	unsigned int nwins; int i; Window w1, w2, *wins;
	if (XQueryTree(display, root, &w1, &w2, &wins, &nwins) && wins)
	{
		for (i = 0; i < nwins; i++)
		{
			XWindowAttributes attr;
			if (XGetWindowAttributes(display, wins[i], &attr)
				&& attr.override_redirect == False
				&& attr.map_state == IsViewable
			) winlist_append(l, wins[i], NULL);
		}
	}
	if (wins) XFree(wins);
	return l;
}

// find the dimensions of the monitor displaying point x,y
void monitor_dimensions(Screen *screen, int x, int y, workarea *mon)
{
	int i; mon->x = 0; mon->y = 0;
	mon->w = WidthOfScreen(screen);
	mon->h = HeightOfScreen(screen);
	mon->l = 0; mon->r = 0; mon->t = 0; mon->b = 0;

	// locate the current monitor
	if (XineramaIsActive(display))
	{
		int monitors;
		XineramaScreenInfo *info = XineramaQueryScreens(display, &monitors);
		if (info) for (i = 0; i < monitors; i++)
		{
			if (x >= info[i].x_org && x < info[i].x_org + info[i].width
				&& y >= info[i].y_org && y < info[i].y_org + info[i].height)
			{
				mon->x = info[i].x_org;
				mon->y = info[i].y_org;
				mon->w = info[i].width;
				mon->h = info[i].height;
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

	// walk the open apps and check for struts
	Window root = RootWindow(display, XScreenNumberOfScreen(screen));
	winlist *wins = windows_in_play(root);
	int i; for (i = 0; i < wins->len; i++)
	{
		XWindowAttributes attr;
		XGetWindowAttributes(display, wins->array[i], &attr);
		if (attr.x >= mon->x && attr.x < mon->x+mon->w
			&& attr.y >= mon->y && attr.y < mon->y+mon->h)
		{
			unsigned long *strut; Atom a; int b;
			unsigned long c, d; unsigned char *res;
			if (XGetWindowProperty(display, wins->array[i], netatoms[NET_WM_STRUT_PARTIAL], 0L, 12,
				False, XA_CARDINAL, &a, &b, &c, &d, &res) == Success && res)
			{
				strut = (unsigned long*)res;
				if (strut[0] && strut[4] >= mon->y && strut[4] < mon->y+mon->h)
					mon->l = MAX(mon->l, strut[0]);
				if (strut[1] && strut[6] >= mon->y && strut[6] < mon->y+mon->h)
					mon->r = MAX(mon->r, strut[1]);
				if (strut[2] && strut[8] >= mon->x && strut[8] < mon->x+mon->w)
					mon->t = MAX(mon->t, strut[2]);
				if (strut[3] && strut[10] >= mon->x && strut[10] < mon->x+mon->w)
					mon->b = MAX(mon->b, strut[3]);
				XFree(res);
			}
		}
	}
	winlist_free(wins);

	mon->x += mon->l; mon->y += mon->t;
	mon->w -= (mon->l+mon->r);
	mon->h -= (mon->t+mon->b);
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
		window_set_atom_prop(c->window, netatoms[NET_WM_STATE], c->state, c->states);
	}
}

void client_remove_state(client *c, Atom state)
{
	if (!client_has_state(c, state)) return;

	Atom newstate[CLIENTSTATE]; int i, n;
	for (i = 0, n = 0; i < c->states; i++) if (c->state[i] != state) newstate[n++] = c->state[i];
	memmove(c->state, newstate, sizeof(Atom)*n); c->states = n;
	window_set_atom_prop(c->window, netatoms[NET_WM_STATE], c->state, c->states);
}

void client_set_state(client *c, Atom state, int on)
{
	if (on) client_add_state(c, state);
	else client_remove_state(c, state);
}

void client_toggle_state(client *c, Atom state)
{
	client_set_state(c, state, !client_has_state(c, state));
}

// collect info on any window
// doesn't have to be a window we'll end up managing
client* window_client(Window win)
{
	if (win == None) return NULL;

	client *c = calloc(sizeof(client), 1);
	c->window = win;

	// if this fails, we're up that creek
	if (!XGetWindowAttributes(display, win, &c->xattr))
		{ free(c); return NULL; }

	XGetTransientForHint(display, win, &c->trans);

	c->visible = c->xattr.map_state == IsViewable ?1:0;
	c->type    = window_get_atom_prop(win, netatoms[NET_WM_WINDOW_TYPE]);
	c->states  = window_get_atom_proplist(win, netatoms[NET_WM_STATE], c->state, CLIENTSTATE);

	if (c->type == None)
	{
		c->type = (c->trans != None)
			// trasients default to dialog
			? netatoms[NET_WM_WINDOW_TYPE_DIALOG]
			// non-transients default to normal
			: netatoms[NET_WM_WINDOW_TYPE_NORMAL];
	}

	c->manage = c->xattr.override_redirect == False
		&& c->type != netatoms[NET_WM_WINDOW_TYPE_DESKTOP]
		&& c->type != netatoms[NET_WM_WINDOW_TYPE_DOCK]
		&& c->type != netatoms[NET_WM_WINDOW_TYPE_SPLASH]
		?1:0;

	c->active = c->manage && c->visible && windows_activated->len
		&& windows_activated->array[windows_activated->len-1] == c->window ?1:0;

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
	int idx = winlist_find(windows, c->window);
	if (idx < 0)
	{
		wincache *cache = calloc(sizeof(wincache), 1);
		winlist_append(windows, c->window, cache);
		idx = windows->len-1;
	}
	c->cache = windows->data[idx];

	return c;
}

// top-level, visible, managed windows. DOES NOT include non-managable docks/panels
winlist* windows_in_play_managed(Window root)
{
	winlist *l = winlist_new();
	unsigned int nwins; int i; Window w1, w2, *wins;
	if (XQueryTree(display, root, &w1, &w2, &wins, &nwins) && wins)
	{
		for (i = 0; i < nwins; i++)
		{
			client *c = window_client(wins[i]);
			if (c && c->manage && c->visible) winlist_append(l, wins[i], NULL);
			free(c);
		}
	}
	if (wins) XFree(wins);
	return l;
}

// extend client data
void client_descriptive_data(client *c)
{
	if (!c || c->title[0] || c->class[0]) return;

	char *name;
	if ((name = window_get_text_prop(c->window, netatoms[NET_WM_NAME])) && name)
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
	char *role = window_get_text_prop(c->window, atoms[WM_WINDOW_ROLE]);
	if (role)
	{
		snprintf(c->role, CLIENTROLE, "%s", role);
		free(role);
	}
}

// extend client data
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
	fprintf(stderr, "%s: %x", e, (unsigned int)w);
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

// debug
void event_client_dump(client *c)
{
	client_descriptive_data(c);
	event_note("%x title: %s", (unsigned int)c->window, c->title);
	event_note("manage:%d input:%d focus:%d", c->manage, c->input, c->focus);
	event_note("class: %s name: %s role: %s", c->class, c->name, c->role);
	event_note("x:%d y:%d w:%d h:%d b:%d override:%d transient:%x", c->xattr.x, c->xattr.y, c->xattr.width, c->xattr.height,
		c->xattr.border_width, c->xattr.override_redirect ?1:0, (unsigned int)c->trans);
	int i, j;
	for (i = 0; i < NETATOMS; i++) if (c->type  == netatoms[i]) event_note("type:%s", netatom_names[i]);
	for (i = 0; i < NETATOMS; i++) for (j = 0; j < c->states; j++) if (c->state[j] == netatoms[i]) event_note("state:%s", netatom_names[i]);
	fflush(stdout);
}

// ewmh_client_list interator data
struct ewmh_client_list_data {
	Atom state1, state2;
	winlist *relevant, *mapped;
};

// ewmh_client_list interator checking NET_WM_STATE
int ewmh_client_list_skip_cb(int i, Window w, void *p)
{
	struct ewmh_client_list_data *search = p;
	client * c = window_client(w);
	if (c && !client_has_state(c, search->state1) && !client_has_state(c, search->state2))
		winlist_append(search->relevant, w, NULL);
	free(c);
	return 0;
}

// ewmh_client_list interator changing relevant windows from focus to mapping order
int ewmh_client_list_mapped_cb(int i, Window w, void *p)
{
	struct ewmh_client_list_data *search = p;
	if (winlist_forget(search->relevant, w))
		winlist_append(search->mapped, w, NULL);
	return 0;
}

// update _NET_CLIENT_LIST
void ewmh_client_list(Window root)
{
	winlist *relevant = winlist_new();
	winlist *mapped   = winlist_new();

	struct ewmh_client_list_data search;
	search.relevant = relevant; search.mapped = mapped;
	search.state1 = netatoms[NET_WM_STATE_SKIP_PAGER];
	search.state2 = netatoms[NET_WM_STATE_SKIP_TASKBAR];

	winlist_iterate_up(windows_activated, ewmh_client_list_skip_cb, &search);
	XChangeProperty(display, root, netatoms[NET_CLIENT_LIST_STACKING], XA_WINDOW, 32, PropModeReplace, (unsigned char*)relevant->array, relevant->len);

	winlist_iterate_up(windows, ewmh_client_list_mapped_cb, &search);
	XChangeProperty(display, root, netatoms[NET_CLIENT_LIST], XA_WINDOW, 32, PropModeReplace, (unsigned char*)mapped->array, mapped->len);

	winlist_free(mapped); winlist_free(relevant);
}

// update _NET_ACTIVE_WINDOW
void ewmh_active_window(Window root, Window w)
{
	XChangeProperty(display, root, netatoms[NET_ACTIVE_WINDOW], XA_WINDOW, 32, PropModeReplace, (unsigned char*)&w, 1);
}

// if a client supports a WM_PROTOCOLS type atom, dispatch an event
int client_protocol_event(client *c, Atom protocol)
{
	Atom *protocols;
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

// focus a window politely if possible
void client_focus(client *c)
{
	client_protocol_event(c, atoms[WM_TAKE_FOCUS]);
	if (c->input) XSetInputFocus(display, c->window, RevertToPointerRoot, CurrentTime);
	else XSetInputFocus(display, PointerRoot, RevertToPointerRoot, CurrentTime);
}

// move & resize a window nicely
void client_moveresize(client *c, int smart, int fx, int fy, int fw, int fh)
{
	client_extended_data(c);

	// this many be different to the client's current c->monitor...
	workarea monitor; monitor_dimensions_struts(c->xattr.screen, fx, fy, &monitor);

	// ensure we match maxv/maxh mode
	if (client_has_state(c, netatoms[NET_WM_STATE_MAXIMIZED_HORZ]))
		{ fx = monitor.x; fw = monitor.w; }
	if (client_has_state(c, netatoms[NET_WM_STATE_MAXIMIZED_VERT]))
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
	// bump onto screen. shrink if necessary
	fw = MAX(1, MIN(fw, monitor.w)); fh = MAX(1, MIN(fh, monitor.h));
	fx = MAX(MIN(fx, monitor.x + monitor.w - fw), monitor.x);
	fy = MAX(MIN(fy, monitor.y + monitor.h - fh), monitor.y);
	fw = MAX(1, MIN(fw, monitor.w - fx + monitor.x));
	fh = MAX(1, MIN(fh, monitor.h - fy + monitor.y));
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
	client_remove_state(c, netatoms[NET_WM_STATE_FULLSCREEN]);

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

// save co-ords for later flip-back
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

struct client_expand_data {
	winlist *inplay; // windows in stacking order
	workarea *regions; // coords/sizes of fully visible windows
	workarea *allregions; // coords/sizes of all windows, visible or obscured
	int relevant; // size of ^regions^ array
	client *main; // the active, focused window we're resizing
};

// client_expand interator
int client_expand_regions(int idx, Window w, void *p)
{
	struct client_expand_data *my = p;
	client *c = window_client(w);
	if (c && c->manage)
	{
		client_extended_data(c);
		// only concerned about windows on this monitor
		if (c->monitor.x == my->main->monitor.x && c->monitor.y == my->main->monitor.y) // same monitor
		{
			client_extended_data(c);
			int i, obscured = 0;
			for (i = my->inplay->len-1; i > idx; i--)
			{
				// if the window intersects with any other window higher in the stack order, it must be at least partially obscured
				if (my->allregions[i].w && INTERSECT(c->sx, c->sy, c->sw, c->sh, my->allregions[i].x, my->allregions[i].y, my->allregions[i].w, my->allregions[i].h))
				{
					obscured = 1;
					break;
				}
			}
			// record a full visible window
			if (!obscured)
			{
				my->regions[my->relevant].x = c->sx;
				my->regions[my->relevant].y = c->sy;
				my->regions[my->relevant].w = c->sw;
				my->regions[my->relevant].h = c->sh;
				my->relevant++;
			}
			my->allregions[idx].x = c->sx;
			my->allregions[idx].y = c->sy;
			my->allregions[idx].w = c->sw;
			my->allregions[idx].h = c->sh;
		}
	}
	free(c);
	return 0;
}

// expand a window to take up available space around it on the current monitor
// do not cover any window that is entirely visible (snap to surrounding edges)
void client_expand(client *c, int directions)
{
	client_extended_data(c);

	struct client_expand_data _my, *my = &_my;
	memset(my, 0, sizeof(struct client_expand_data));

	my->inplay = windows_activated;
	// list of coords/sizes for fully visible windows
	my->regions = calloc(sizeof(workarea), my->inplay->len);
	// list of coords/sizes for all relevant windows
	my->allregions = calloc(sizeof(workarea), my->inplay->len);
	my->main = c;

	// build the (all)regions arrays
	winlist_iterate_down(my->inplay, client_expand_regions, my);

	int i, n, x = c->sx, y = c->sy, w = c->sw, h = c->sh;

	if (directions & VERTICAL)
	{
		// try to grow upward. locate the lower edge of the nearest fully visible window
		n = 0;
		for (i = 1; i < my->relevant; i++)
		{
			if (my->regions[i].y + my->regions[i].h <= y && OVERLAP(x, w, my->regions[i].x, my->regions[i].w))
				n = MAX(n, my->regions[i].y + my->regions[i].h);
		}
		h += y-n; y = n;
		// try to grow downward. locate the upper edge of the nearest fully visible window
		n = c->monitor.h;
		for (i = 1; i < my->relevant; i++)
		{
			if (my->regions[i].y >= y+h && OVERLAP(x, w, my->regions[i].x, my->regions[i].w))
				n = MIN(n, my->regions[i].y);
		}
		h = n-y;
	}
	if (directions & HORIZONTAL)
	{
		// try to grow left. locate the right edge of the nearest fully visible window
		n = 0;
		for (i = 1; i < my->relevant; i++)
		{
			if (my->regions[i].x + my->regions[i].w <= x && OVERLAP(y, h, my->regions[i].y, my->regions[i].h))
				n = MAX(n, my->regions[i].x + my->regions[i].w);
		}
		w += x-n; x = n;
		// try to grow right. locate the left edge of the nearest fully visible window
		n = c->monitor.w;
		for (i = 1; i < my->relevant; i++)
		{
			if (my->regions[i].x >= x+w && OVERLAP(y, h, my->regions[i].y, my->regions[i].h))
				n = MIN(n, my->regions[i].x);
		}
		w = n-x;
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
		client_moveresize(c, 0, c->monitor.x+x, c->monitor.y+y, w, h);
	}
	free(my->regions); free(my->allregions);
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
	if (c->trans)
	{
		free(a); a = window_client(c->trans);
		if (a && a->manage) app = a->window;
	}

	if (app != orig) winlist_append(stack, orig, NULL);

	// locate all visible transient windows for this app
	winlist *inplay = windows_in_play(c->xattr.root);
	for (i = inplay->len-1; i > -1; i--)
	{
		if (inplay->array[i] == app) continue;
		free(a); a = window_client(inplay->array[i]);
		if (a && a->trans == app) winlist_append(stack, a->window, NULL);
	}
	winlist_free(inplay);
	winlist_append(stack, app, NULL);
	free(a);
}

// client_raise iterator data
struct client_raise_data {
	Atom type;
	Atom state;
	winlist *stack;
};

// client_raise interator checking NET_WM_STATE
int client_raise_state_cb(int i, Window w, void *p)
{
	struct client_raise_data *search = p;
	if (winlist_find(search->stack, w) < 0)
	{
		client *o = window_client(w);
		if (o && o->trans == None && client_has_state(o, search->state))
			client_stack_family(o, search->stack);
		free(o);
	}
	return 0;
}

// client_raise interator checking NET_WM_WINDOW_TYPE
int client_raise_type_cb(int i, Window w, void *p)
{
	struct client_raise_data *search = p;
	if (winlist_find(search->stack, w) < 0)
	{
		client *o = window_client(w);
		if (o && o->trans == None && o->type == search->type)
			client_stack_family(o, search->stack);
		free(o);
	}
	return 0;
}

// raise a window and its transients
void client_raise(client *c, int priority)
{
	winlist *stack = winlist_new();

	// priority gets us raised without anyone above us, regardless. eg _NET_WM_STATE_FULLSCREEN+focus
	if (!priority)
	{
		// locate windows with _NET_WM_STATE_ABOVE and/or _NET_WM_WINDOW_TYPE_DOCK to raise them first
		struct client_raise_data search; search.stack = stack;
		winlist *inplay = windows_in_play(c->xattr.root);
		search.state = netatoms[NET_WM_STATE_ABOVE];
		winlist_iterate_down(inplay, client_raise_state_cb, &search);
		search.type = netatoms[NET_WM_WINDOW_TYPE_DOCK];
		winlist_iterate_down(inplay, client_raise_type_cb, &search);
		winlist_free(inplay);
	}
	// locate our family
	if (winlist_find(stack, c->window) < 0)
		client_stack_family(c, stack);

	// raise the top window inthe stack
	XRaiseWindow(display, stack->array[0]);
	// stack everything else, in order, underneath top window
	if (stack->len > 1) XRestackWindows(display, stack->array, stack->len);

	winlist_free(stack);
}

struct tag_raise_data {
	unsigned int tag;
	winlist *stack;
};

// tag_raise iterator
int tag_raise_cb(int idx, Window w, void *p)
{
	struct tag_raise_data *my = p;
	client *c = window_client(w);
	if (c && c->manage && winlist_find(my->stack, w) < 0)
	{
		if (c->cache->tags & my->tag)
			client_stack_family(c, my->stack);
	}
	free(c);
	return 0;
}

// raise all windows in a tag
void tag_raise(unsigned int tag)
{
	struct tag_raise_data _my, *my = &_my;
	memset(my, 0, sizeof(struct tag_raise_data));

	my->tag = tag;
	my->stack = winlist_new();

	// locate windows with _NET_WM_STATE_ABOVE and/or _NET_WM_WINDOW_TYPE_DOCK to raise them first
	struct client_raise_data search; search.stack = my->stack;
	search.state = netatoms[NET_WM_STATE_ABOVE];
	winlist_iterate_down(windows_activated, client_raise_state_cb, &search);
	search.type = netatoms[NET_WM_WINDOW_TYPE_DOCK];
	winlist_iterate_down(windows_activated, client_raise_type_cb, &search);

	// locate all windows in the tag
	winlist_iterate_down(windows_activated, tag_raise_cb, my);

	if (my->stack->len)
	{
		// raise the top window in the stack
		XRaiseWindow(display, my->stack->array[0]);
		// stack everything else, in order, underneath top window
		if (my->stack->len > 1) XRestackWindows(display, my->stack->array, my->stack->len);
	}
	winlist_free(my->stack);
	currenttag = tag;
}

// toggle client in current tag
void client_toggle_tag(client *c, unsigned int tag)
{
	if (c->cache->tags & tag)
	{
		c->cache->tags &= ~tag;
		client_flash(c, config_flash_off, config_flash_ms);
	}
	else
	{
		c->cache->tags |= tag;
		client_flash(c, config_flash_on, config_flash_ms);
	}
}

// if client is new or has changed state since we last looked, tweak stuff
void client_review(client *c)
{
	client_extended_data(c);

	int is_full = (c->xattr.x - c->monitor.x) < 1
		&& (c->xattr.y - c->monitor.y) < 1
		&& c->xattr.width  >= c->monitor.w
		&& c->xattr.height >= c->monitor.h
		? 1: 0;

	XSetWindowBorderWidth(display, c->window, is_full ? 0:config_border_width);

	Atom allowed[7] = {
		netatoms[NET_WM_ACTION_MOVE],
		netatoms[NET_WM_ACTION_RESIZE],
		netatoms[NET_WM_ACTION_FULLSCREEN],
		netatoms[NET_WM_ACTION_CLOSE],
		netatoms[NET_WM_ACTION_STICK],
		netatoms[NET_WM_ACTION_MAXIMIZE_HORZ],
		netatoms[NET_WM_ACTION_MAXIMIZE_VERT],
	};

	window_set_atom_prop(c->window, netatoms[NET_WM_ALLOWED_ACTIONS], allowed, 7);

	if (c->cache && !c->is_full)
	{
		// if client is in a screen corner, track it...
		// if we shrink the window form maxv/maxh/fullscreen later,
		// we can have it stick to the original corner rather then re-centering
		if (c->is_left  && c->is_top) c->cache->last_corner = TOPLEFT;
		else if (c->is_left  && c->is_bottom) c->cache->last_corner = BOTTOMLEFT;
		else if (c->is_right && c->is_top) c->cache->last_corner = TOPRIGHT;
		else if (c->is_right && c->is_bottom) c->cache->last_corner = BOTTOMRIGHT;
		else c->cache->last_corner = 0;
	}
}

// client_activate deactivate iterator
int client_activate_cb(int i, Window w, void *p)
{
	if (w != *((Window*)p))
		XSetWindowBorder(display, w, config_border_blur);
	return 0;
}

// raise and focus a client
void client_activate(client *c)
{
	// deactivate everyone else
	winlist *wins = windows_in_play_managed(c->xattr.root);
	winlist_iterate_up(wins, client_activate_cb, &c->window);
	winlist_free(wins);
	// setup ourself
	client_review(c);
	client_raise(c, client_has_state(c, netatoms[NET_WM_STATE_FULLSCREEN]));
	client_focus(c);
	XSetWindowBorder(display, c->window, config_border_focus);
	// update focus history order
	winlist_forget(windows_activated, c->window);
	winlist_append(windows_activated, c->window, NULL);
	ewmh_active_window(c->xattr.root, c->window);
	// tell the user something happened
	if (!c->active) client_flash(c, config_border_focus, config_flash_ms);
}

// set WM_STATE
void client_state(client *c, long state)
{
	long payload[] = { state, None };
	XChangeProperty(display, c->window, atoms[WM_STATE], atoms[WM_STATE], 32, PropModeReplace, (unsigned char*)payload, 2);
	if (state == WithdrawnState)
	{
		window_unset_prop(c->window, netatoms[NET_WM_STATE]);
		winlist_forget(windows_activated, c->window);
	}
}

// window_active_client interator state
struct window_active_client_data {
	client *cli;
	Window root;
};

// window_active_client iterator
int window_active_client_cb(int i, Window w, void *p)
{
	struct window_active_client_data *data = p;
	client *c = window_client(w);
	if (c && c->manage && c->visible && c->xattr.root == data->root)
		{ data->cli = c; return 1; }
	free(c); return 0;
}

// locate the currently focused window and build a client for it
client* window_active_client(Window root)
{
	struct window_active_client_data search;
	search.cli = NULL; search.root = root;
	// look for a visible, previously activated window
	winlist_iterate_down(windows_activated, window_active_client_cb, &search);
	// if we found one, activate it
	if (search.cli && (!search.cli->focus || !search.cli->active))
		client_activate(search.cli);
	// otherwise look for any visible, manageable window
	if (!search.cli)
	{
		winlist *inplay = windows_in_play(root);
		winlist_iterate_down(inplay, window_active_client_cb, &search);
		if (search.cli) client_activate(search.cli);
		winlist_free(inplay);
	}
	return search.cli;
}

// determine which monitor holds the active window, or failing that the mouse pointer
void monitor_active(Screen *screen, workarea *mon)
{
	Window root = RootWindow(display, XScreenNumberOfScreen(screen));
	client *c = window_active_client(root);
	if (c)
	{
		client_extended_data(c);
		memmove(mon, &c->monitor, sizeof(workarea));
		free(c);
		return;
	}
	Window rr, cr; int rxr, ryr, wxr, wyr; unsigned int mr;
	if (XQueryPointer(display, root, &rr, &cr, &rxr, &ryr, &wxr, &wyr, &mr))
	{
		monitor_dimensions_struts(screen, rxr, ryr, mon);
		return;
	}
	monitor_dimensions_struts(screen, 0, 0, mon);
}

// go fullscreen on current monitor
void client_nws_fullscreen(client *c, int action)
{
	int state = client_has_state(c, netatoms[NET_WM_STATE_FULLSCREEN]);

	if (action == ADD || (action == TOGGLE && !state))
	{
		client_save_position(c);
		// no struts!
		workarea monitor; monitor_dimensions(c->xattr.screen, c->xattr.x, c->xattr.y, &monitor);
		client_set_state(c, netatoms[NET_WM_STATE_FULLSCREEN], 1);
		// not client_moveresize! that would get tricky and recheck struts
		XMoveResizeWindow(display, c->window, monitor.x, monitor.y, monitor.w, monitor.h);
	}
	else
	if (action == REMOVE || (action == TOGGLE && state))
	{
		client_extended_data(c);
		client_restore_position(c, 0, c->monitor.x + (c->monitor.w/4), c->monitor.y + (c->monitor.h/4), c->monitor.w/2, c->monitor.h/2);
	}
	// fullscreen may need to hide above windows
	if (c->active) client_activate(c);
}

// raise above other windows
void client_nws_above(client *c, int action)
{
	int state = client_has_state(c, netatoms[NET_WM_STATE_ABOVE]);

	if (action == ADD || (action == TOGGLE && !state))
	{
		client_add_state(c, netatoms[NET_WM_STATE_ABOVE]);
		client_raise(c, 0);
		client_flash(c, config_flash_on, config_flash_ms);
	}
	else
	if (action == REMOVE || (action == TOGGLE && state))
	{
		client_remove_state(c, netatoms[NET_WM_STATE_ABOVE]);
		client_flash(c, config_flash_off, config_flash_ms);
	}
}

// maximize vertically
void client_nws_maxvert(client *c, int action)
{
	client_extended_data(c);
	int state = client_has_state(c, netatoms[NET_WM_STATE_MAXIMIZED_VERT]);

	if (action == ADD || (action == TOGGLE && !state))
	{
		client_save_position_vert(c);
		client_add_state(c, netatoms[NET_WM_STATE_MAXIMIZED_VERT]);
		client_moveresize(c, 1, c->x, c->y, c->sw, c->monitor.h);
		client_flash(c, config_flash_on, config_flash_ms);
	}
	else
	if (action == REMOVE || (action == TOGGLE && state))
	{
		client_remove_state(c, netatoms[NET_WM_STATE_MAXIMIZED_VERT]);
		client_restore_position_vert(c, 0, c->monitor.y + (c->monitor.h/4), c->monitor.h/2);
		client_flash(c, config_flash_off, config_flash_ms);
	}
}

// maximize horizontally
void client_nws_maxhorz(client *c, int action)
{
	client_extended_data(c);
	int state = client_has_state(c, netatoms[NET_WM_STATE_MAXIMIZED_HORZ]);

	if (action == ADD || (action == TOGGLE && !state))
	{
		client_save_position_horz(c);
		client_add_state(c, netatoms[NET_WM_STATE_MAXIMIZED_HORZ]);
		client_moveresize(c, 1, c->x, c->y, c->monitor.w, c->sh);
		client_flash(c, config_flash_on, config_flash_ms);
	}
	else
	if (action == REMOVE || (action == TOGGLE && state))
	{
		client_remove_state(c, netatoms[NET_WM_STATE_MAXIMIZED_HORZ]);
		client_restore_position_horz(c, 0, c->monitor.x + (c->monitor.w/4), c->monitor.w/2);
		client_flash(c, config_flash_off, config_flash_ms);
	}
}

// iterator data for app_find_by_field()
struct app_find {
	char *pattern;
	Window found;
	int offset;
};

// iterator for app_find_or_start()
int app_find_by_field(int i, Window w, void *p)
{
	struct app_find *find = p;
	int r = 0; client *c = window_client(w);
	if (c && c->manage && c->visible)
	{
		client_descriptive_data(c);
		if (!strcasecmp(((char*)c)+find->offset, find->pattern))
			{ find->found = w; r = 1; }
	}
	free(c); return r;
}

// search and activate first open window matching class/name/role/title
void app_find_or_start(Window root, char *pattern)
{
	if (!pattern) return;

	struct app_find find;
	find.pattern = pattern;
	find.found   = None;

	if (find.found == None)
	{
		find.offset = offsetof(client, name);
		winlist_iterate_down(windows_activated, app_find_by_field, &find);
	}
	if (find.found == None)
	{
		find.offset = offsetof(client, role);
		winlist_iterate_down(windows_activated, app_find_by_field, &find);
	}
	if (find.found == None)
	{
		find.offset = offsetof(client, class);
		winlist_iterate_down(windows_activated, app_find_by_field, &find);
	}
	if (find.found == None)
	{
		find.offset = offsetof(client, title);
		winlist_iterate_down(windows_activated, app_find_by_field, &find);
	}
	client *c = NULL;
	if (find.found != None && (c = window_client(find.found)) && c && c->manage && c->visible)
		client_activate(c);
	else exec_cmd(pattern);
	free(c);
}

struct window_switcher_data {
	unsigned int tag; // filter by tag
	int classfield;  // width of WM_CLASS field
	char *list; // output window list
	int used;   // out list length
	char pattern[20]; // window line format
};

// window_switcher interator
int window_switcher_cb_class(int idx, Window w, void *p)
{
	struct window_switcher_data *my = p;
	client *c = window_client(w);
	if (c)
	{
		client_descriptive_data(c);
		if (!my->tag || (c->cache && c->cache->tags & my->tag))
			my->classfield = MAX(my->classfield, strlen(c->class));
	}
	free(c);
	return 0;
}

// window_switcher interator
int window_switcher_cb_format(int idx, Window w, void *p)
{
	struct window_switcher_data *my = p;
	client *c = window_client(w);
	if (c && !client_has_state(c, netatoms[NET_WM_STATE_SKIP_PAGER]
		&& !client_has_state(c, netatoms[NET_WM_STATE_SKIP_TASKBAR])))
	{
		client_descriptive_data(c);
		if (!my->tag || (c->cache && c->cache->tags & my->tag))
		{
			my->list = realloc(my->list, my->used + strlen(c->title) + strlen(c->class) + my->classfield + 20);
			my->used += sprintf(my->list + my->used, my->pattern, (unsigned long)c->window, c->class, c->title);
		}
	}
	free(c);
	return 0;
}

// built-in window switcher
void window_switcher(Window root, unsigned int tag)
{
	struct window_switcher_data my; my.classfield = 0; my.tag = tag;
	winlist_iterate_down(windows_activated, window_switcher_cb_class, &my);
	my.list = calloc(1, 1024); my.used = 0;
	sprintf(my.pattern, "%%08lx  %%%ds  %%s\n", MAX(5, my.classfield));
	winlist_iterate_down(windows_activated, window_switcher_cb_format, &my);

	int in, out;
	pid_t pid = fork();
	if (!pid)
	{
		display = XOpenDisplay(0);
		pid = exec_cmd_io(SWITCHER_BUILTIN, &in, &out);
		if (pid > 0)
		{
			write(in, my.list, my.used); close(in);
			my.used = read(out, my.list, 10);
			close(out);
			if (my.used > 0)
			{
				my.list[my.used] = '\0';
				Window w = (Window)strtol(my.list, NULL, 16);
				window_send_message(root, w, netatoms[NET_ACTIVE_WINDOW], 2, // 2 = pager
					SubstructureNotifyMask | SubstructureRedirectMask);
			}
		}
		waitpid(pid, NULL, 0);
		exit(EXIT_SUCCESS);
	}
	free(my.list);
}

// MODKEY+keys
void handle_keypress(XEvent *ev)
{
	event_log("KeyPress", ev->xany.window);
	XWindowAttributes attr;

	// this will be a root window as we only bind buttons on those
	XGetWindowAttributes(display, ev->xany.window, &attr);
	KeySym key = XkbKeycodeToKeysym(display, ev->xkey.keycode, 0, 0);

	client *c = NULL;

	if (key == XK_Tab)
	{
		if (config_switcher) exec_cmd(config_switcher);
		else window_switcher(ev->xany.window, 0);
	}
	// custom MODKEY launchers
	// on the command line: goomwwm -1 "firefox"
	else if (key == XK_1) app_find_or_start(attr.root, config_key_1);
	else if (key == XK_2) app_find_or_start(attr.root, config_key_2);
	else if (key == XK_3) app_find_or_start(attr.root, config_key_3);
	else if (key == XK_4) app_find_or_start(attr.root, config_key_4);
	else if (key == XK_5) app_find_or_start(attr.root, config_key_5);
	else if (key == XK_6) app_find_or_start(attr.root, config_key_6);
	else if (key == XK_7) app_find_or_start(attr.root, config_key_7);
	else if (key == XK_8) app_find_or_start(attr.root, config_key_8);
	else if (key == XK_9) app_find_or_start(attr.root, config_key_9);

	else if (key == XK_F1) tag_raise(TAG1);
	else if (key == XK_F2) tag_raise(TAG2);
	else if (key == XK_F3) tag_raise(TAG3);
	else if (key == XK_F4) tag_raise(TAG4);
	else if (key == XK_F5) tag_raise(TAG5);
	else if (key == XK_F6) tag_raise(TAG6);
	else if (key == XK_F7) tag_raise(TAG7);
	else if (key == XK_F8) tag_raise(TAG8);
	else if (key == XK_F9) tag_raise(TAG9);

	else
	// following only relevant with a focused window
	if ((c = window_active_client(attr.root)) && c)
	{
		client_descriptive_data(c);
		client_extended_data(c);

		int screen_x = c->monitor.x, screen_y = c->monitor.y;
		int screen_width = c->monitor.w, screen_height = c->monitor.h;
		int vague = screen_width/100;

		// window co-ords translated to 0-based on screen
		int x = c->sx; int y = c->sy; int w = c->sw; int h = c->sh;

		// four basic window sizes
		// full screen
		int width1  = screen_width/3;
		int height1 = screen_height/3;
		int width2  = screen_width/2;
		int height2 = screen_height/2;
		int width3  = screen_width-width1;
		int height3 = screen_height-height1;
		int width4  = screen_width;
		int height4 = screen_height;

		// final resize/move params. smart = intelligently bump / center / shrink
		int fx = 0, fy = 0, fw = 0, fh = 0, smart = 0;

		if (key == XK_Escape) client_close(c);
		else if (key == XK_i) event_client_dump(c);
		else if (key == XK_x) exec_cmd(config_launcher);
		else if (key == XK_t) client_toggle_tag(c, currenttag);
		else if (key == XK_equal) client_nws_above(c, TOGGLE);
		else if (key == XK_backslash) client_nws_fullscreen(c, TOGGLE);
		else if (key == XK_bracketleft) client_nws_maxhorz(c, TOGGLE);
		else if (key == XK_bracketright) client_nws_maxvert(c, TOGGLE);
		else if (key == XK_Return) client_expand(c, HORIZONTAL|VERTICAL);
		else if (key == XK_semicolon) client_expand(c, HORIZONTAL);
		else if (key == XK_apostrophe) client_expand(c, VERTICAL);
		else
		// cycle through windows with same WM_CLASS
		if (key == XK_grave)
			window_switcher(c->xattr.root, currenttag);
		else
		if (key == XK_Page_Up || key == XK_Page_Down || key == XK_Home || key == XK_End || key == XK_Insert || key == XK_Delete)
		{
			smart = 1;
			// window width zone
			int isw4 = w >= width4 ?1:0;
			int isw3 = !isw4 && w >= width3 ?1:0;
			int isw2 = !isw4 && !isw3 && w >= width2 ?1:0;
			int isw1 = !isw4 && !isw3 && !isw2 && w >= width1 ?1:0;
			int isw0 = !isw4 && !isw3 && !isw2 && !isw1 ?1:0;

			// window height zone
			int ish4 = h >= height4 ?1:0;
			int ish3 = !ish4 && h >= height3 ?1:0;
			int ish2 = !ish4 && !ish3 && h >= height2 ?1:0;
			int ish1 = !ish4 && !ish3 && !ish2 && h >= height1 ?1:0;
			int ish0 = !ish4 && !ish3 && !ish2 && !ish1 ?1:0;

			// window zone ballpark. try to make resize changes intuitive base on window area
			int is4 = (isw4 && ish4) || (w*h >= width4*height4) ?1:0;
			int is3 = !is4 && ((isw3 && ish3) || (w*h >= width3*height3)) ?1:0;
			int is2 = !is4 && !is3 && ((isw2 && ish2) || (w*h >= width2*height2)) ?1:0;
			int is1 = !is4 && !is3 && !is2 && ((isw1 && ish1) || (w*h >= width1*height1)) ?1:0;
			int is0 = !is4 && !is3 && !is2 && !is1 ?1:0;

			// Page Up/Down makes the focused window larger and smaller respectively
			if (key == XK_Page_Up)
			{
				fx = screen_x + c->sx; fy = screen_y + c->sy;
				if (is0) { fw = width1; fh = height1; }
				else if (is1) { fw = width2; fh = height2; }
				else if (is2) { fw = width3; fh = height3; }
				else if (is3) { fw = width4; fh = height4; }
				else { fw = width4; fh = height4; }
				client_remove_state(c, netatoms[NET_WM_STATE_MAXIMIZED_HORZ]);
				client_remove_state(c, netatoms[NET_WM_STATE_MAXIMIZED_VERT]);
				smart = 1;
			}
			else
			if (key == XK_Page_Down)
			{
				fx = screen_x + c->sx; fy = screen_y + c->sy;
				if (is4) { fw = width3; fh = height3; }
				else if (is3) { fw = width2; fh = height2; }
				else { fw = width1; fh = height1; }
				client_remove_state(c, netatoms[NET_WM_STATE_MAXIMIZED_HORZ]);
				client_remove_state(c, netatoms[NET_WM_STATE_MAXIMIZED_VERT]);
				smart = 1;
			}
			else
			// Home/End makes the focused window HEIGHT larger and smaller respectively
			if (key == XK_Home || key == XK_End)
			{
				fx = screen_x + c->sx; fy = screen_y + c->sy; fw = c->sw;
				if (key == XK_Home) { if (ish0) fh = height1; else if (ish1) fh = height2; else if (ish2) fh = height3; else fh = height4; }
				if (key == XK_End)  { if (ish4) fh = height3; else if (ish3) fh = height2; else fh = height1; }
				client_remove_state(c, netatoms[NET_WM_STATE_MAXIMIZED_VERT]);
				smart = 1;
			}
			else
			// Insert/Delete makes the focused window WIDTH larger and smaller respectively
			if (key == XK_Insert || key == XK_Delete)
			{
				fx = screen_x + c->sx; fy = screen_y + c->sy; fh = c->sh;
				if (key == XK_Insert) { if (isw0) fw = width1; else if (isw1) fw = width2; else if (isw2) fw = width3; else fw = width4; }
				if (key == XK_Delete) { if (isw4) fw = width3; else if (isw3) fw = width2; else fw = width1; }
				client_remove_state(c, netatoms[NET_WM_STATE_MAXIMIZED_HORZ]);
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

			// monitor switching if window is on an edge
			if (key == XK_Left && c->is_left)
			{
				monitor_dimensions_struts(c->xattr.screen, c->x-vague, c->y, &mon);
				if (mon.x < c->monitor.x) { fx = mon.x+mon.w-w; fy = y; fw = w; fh = h; }
			}
			else
			if (key == XK_Right && c->is_right)
			{
				monitor_dimensions_struts(c->xattr.screen, c->x+c->sw+vague, c->y, &mon);
				if (mon.x > c->monitor.x) { fx = mon.x; fy = y; fw = w; fh = h; }
			}
			else
			if (key == XK_Up && c->is_top)
			{
				monitor_dimensions_struts(c->xattr.screen, c->x, c->y-vague, &mon);
				if (mon.y < c->monitor.y) { fx = x; fy = mon.y+mon.h-h; fw = w; fh = h; }
			}
			else
			if (key == XK_Down && c->is_bottom)
			{
				monitor_dimensions_struts(c->xattr.screen, c->x, c->y+c->sh+vague, &mon);
				if (mon.y > c->monitor.y) { fx = x; fy = mon.y; fw = w; fh = h; }
			}
			else
			// MODKEY+Arrow movement occurs on a 3x3 grid for non-fullscreen, managed windows
			if (!c->is_full)
			{
				// move within current monitor
				if (key == XK_Left && !c->is_maxh)
					{ fx = screen_x + (wx > (screen_width/2)+vague ? cx: 0); fy = screen_y+y; fw = w; fh = h; }
				else
				if (key == XK_Right && !c->is_maxh)
					{ fx = screen_x + (wx < (screen_width/2)-vague ? cx: screen_width - w); fy = screen_y+y; fw = w; fh = h; }
				else
				if (key == XK_Up && !c->is_maxv)
					{ fx = screen_x+x; fy = screen_y + (wy > (screen_height/2)+vague ? cy: 0); fw = w; fh = h; }
				else
				if (key == XK_Down && !c->is_maxv)
					{ fx = screen_x+x; fy = screen_y + (wy < (screen_height/2)-vague ? cy: screen_height - h); fw = w; fh = h; }
			}
		}
		// final co-ords are fixed. go to it...
		if (fw > 0 && fh > 0) client_moveresize(c, smart, fx, fy, fw, fh);
	}
	if (c) free(c);
}

// MODKEY+keys
void handle_keyrelease(XEvent *ev)
{
	event_log("KeyRelease", ev->xany.window);
}

// we bind on all mouse buttons on the root window to implement click-to-focus
// events are compressed, checked for a window change, then replayed through to clients
void handle_buttonpress(XEvent *ev)
{
	event_log("ButtonPress", ev->xbutton.subwindow);
	// all mouse button events except the wheel come here, so we can click-to-focus
	// turn off caps and num locks bits. dont care about their states
	int state = ev->xbutton.state & ~(LockMask|NumlockMask); client *c = NULL;

	if (ev->xbutton.subwindow != None && (c = window_client(ev->xbutton.subwindow)) && c && c->manage)
	{
		// check mod4 is the only modifier. if so, it's a legit move/resize event
		if (state & config_modkey && !(state & config_ignore_modkeys))
		{
			XGrabPointer(display, c->window, True, PointerMotionMask|ButtonReleaseMask,
				GrabModeAsync, GrabModeAsync, None, None, CurrentTime);
			memcpy(&mouse_attr, &c->xattr, sizeof(c->xattr));
			memcpy(&mouse_button, &ev->xbutton, sizeof(ev->xbutton));
		}
		else
		{
			if (!c->focus || !c->active) client_activate(c);
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
	free(c);
}

// only get these if a window move/resize has been started in buttonpress
void handle_buttonrelease(XEvent *ev)
{
	event_log("ButtonRelease", ev->xbutton.subwindow);
	XUngrabPointer(display, CurrentTime);
}

// only get these if a window move/resize has been started in buttonpress
void handle_motionnotify(XEvent *ev)
{
	event_log("MotionNotify", ev->xbutton.subwindow);
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
		if (mouse_button.button == Button1)
		{
			if (NEAR(c->monitor.x, vague, x)) x = c->monitor.x;
			if (NEAR(c->monitor.y, vague, y)) y = c->monitor.y;
			if (NEAR(c->monitor.x+c->monitor.w, vague, x+w)) x = c->monitor.x+c->monitor.w-w-(config_border_width*2);
			if (NEAR(c->monitor.y+c->monitor.h, vague, y+h)) y = c->monitor.y+c->monitor.h-h-(config_border_width*2);
		}
		else
		if (mouse_button.button == Button3)
		{
			if (NEAR(c->monitor.x+c->monitor.w, vague, x+w)) w = c->monitor.x+c->monitor.w-x-(config_border_width*2);
			if (NEAR(c->monitor.y+c->monitor.h, vague, y+h)) h = c->monitor.y+c->monitor.h-y-(config_border_width*2);
		}
		XMoveResizeWindow(display, ev->xmotion.window, x, y, w, h);

		// who knows where we've ended up. clear states
		client_remove_state(c, netatoms[NET_WM_STATE_MAXIMIZED_HORZ]);
		client_remove_state(c, netatoms[NET_WM_STATE_MAXIMIZED_VERT]);
	}
	free(c);
}

// we dont really care until a window configures and maps, so just watch it
void handle_createnotify(XEvent *ev)
{
	event_log("CreateNotify", ev->xcreatewindow.window);
	XSelectInput(display, ev->xcreatewindow.window, EnterWindowMask | LeaveWindowMask | FocusChangeMask | PropertyChangeMask);
	if (winlist_find(windows, ev->xcreatewindow.window) < 0)
	{
		wincache *cache = calloc(sizeof(wincache), 1);
		winlist_append(windows, ev->xcreatewindow.window, cache);
	}
	if (window_is_root(ev->xcreatewindow.parent)) ewmh_client_list(ev->xcreatewindow.parent);
}

// we don't track window state internally much, so this is just for info
void handle_destroynotify(XEvent *ev)
{
	event_log("DestroyNotify", ev->xdestroywindow.window);
	winlist_forget(windows, ev->xdestroywindow.window);
	winlist_forget(windows_activated, ev->xdestroywindow.window);
	if (window_is_root(ev->xdestroywindow.event)) ewmh_client_list(ev->xdestroywindow.event);
}

// very loose with configure requests
// just let stuff go through mostly unchanged so apps can remember window positions/sizes
void handle_configurerequest(XEvent *ev)
{
	event_log("ConfigureRequest", ev->xconfigurerequest.window);
	client *c = window_client(ev->xconfigurerequest.window);
	if (c)
	{
		XConfigureRequestEvent *e = &ev->xconfigurerequest;

		XWindowChanges wc;
		wc.x = e->x; wc.y = e->y; wc.width = e->width; wc.height = e->height;
		wc.border_width = 0; wc.sibling = None; wc.stack_mode = None;

		// only move/resize requests go through. never stacking
		if (e->value_mask & (CWX|CWY|CWWidth|CWHeight))
		{
			client_extended_data(c);
			unsigned long mask = e->value_mask & (CWX|CWY|CWWidth|CWHeight);
			// if we previously instructed the window to an x/y/w/h which conforms to
			// their w/h hints, demand co-operation!
			if (c->cache && c->cache->have_mr)
			{
				mask = CWX|CWY|CWWidth|CWHeight;
				wc.x = c->cache->mr_x; wc.y = c->cache->mr_y;
				wc.width  = c->cache->mr_w; wc.height = c->cache->mr_h;
			}
			XConfigureWindow(display, c->window, mask, &wc);
		}
	}
	free(c);
}

// once a window has been configured, apply a border unless it is fullscreen
void handle_configurenotify(XEvent *ev)
{
	event_log("ConfigureNotify", ev->xconfigure.window);
	client *c = window_client(ev->xconfigure.window);
	if (c && c->manage) client_review(c);
	free(c);
}

// map requests are when we get nasty about co-ords and size
void handle_maprequest(XEvent *ev)
{
	event_log("MapRequest", ev->xmaprequest.window);
	client *c = window_client(ev->xmaprequest.window);
	if (c && c->manage)
	{
		client_state(c, NormalState);
		client_extended_data(c);

		if (!client_has_state(c, netatoms[NET_WM_STATE_STICKY]) && !c->x && !c->y)
		{
			// adjust for borders on remembered co-ords
			if (c->type == netatoms[NET_WM_WINDOW_TYPE_NORMAL])
				{ c->w += config_border_width*2; c->h += config_border_width*2; }

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
			free(p);
		}
		client_raise(c, 0);
	}
	XMapWindow(display, c->window);
	free(c);
}

// a newly mapped top-level window automatically takes focus
// this could be configurable?
void handle_mapnotify(XEvent *ev)
{
	event_log("MapNotify", ev->xmap.window);
	client *c = window_client(ev->xmap.window);
	if (c && c->manage)
	{
		client_activate(c);
		ewmh_client_list(c->xattr.root);
	}
	free(c);
}

// unmapping could indicate the focus window has closed
// find a new one to focus if needed
void handle_unmapnotify(XEvent *ev)
{
	event_log("UnmapNotify", ev->xunmap.window);
	client *c = window_client(ev->xunmap.window);
	// was it a top-level app window that closed?
	if (c && c->manage) client_state(c, WithdrawnState);
	// was it the active window?
	if (window_is_root(ev->xunmap.event) && windows_activated->len
		&& ev->xunmap.window == windows_activated->array[windows_activated->len-1])
		free(window_active_client(ev->xunmap.event));
	free(c);
}

// ClientMessage
void handle_clientmessage(XEvent *ev)
{
	event_log("ClientMessage", ev->xclient.window);
	XClientMessageEvent *m = &ev->xclient;
	client *c = window_client(m->window);

	if (c && c->manage && c->visible)
	{
		if (m->message_type == netatoms[NET_ACTIVE_WINDOW])
			client_activate(c);

		else
		if (m->message_type == netatoms[NET_CLOSE_WINDOW])
			client_close(c);

		else
		if (m->message_type == netatoms[NET_MOVERESIZE_WINDOW] &&
			(m->data.l[1] >= 0 || m->data.l[2] >= 0 || m->data.l[3] > 0 || m->data.l[4] > 0))
		{
			client_extended_data(c);
			client_moveresize(c, 0,
				m->data.l[1] >= 0 ? m->data.l[1]: c->x,
				m->data.l[2] >= 0 ? m->data.l[2]: c->y,
				m->data.l[3] >= 1 ? m->data.l[3]: c->sw,
				m->data.l[4] >= 1 ? m->data.l[4]: c->sh);
		}
		else
		if (m->message_type == netatoms[NET_WM_STATE])
		{
			int i; for (i = 1; i < 2; i++)
			{
				if (m->data.l[i] == netatoms[NET_WM_STATE_FULLSCREEN])
					client_nws_fullscreen(c, m->data.l[0]);
				else
				if (m->data.l[i] == netatoms[NET_WM_STATE_ABOVE])
					client_nws_above(c, m->data.l[0]);
			}
		}
	}
	free(c);
}

// PropertyNotify
void handle_propertynotify(XEvent *ev)
{
	event_log("PropertyNotify", ev->xproperty.window);
	XPropertyEvent *p = &ev->xproperty;
	client *c = window_client(p->window);

	if (c && c->manage && c->visible)
	{
		if (p->atom == atoms[WM_NAME] || p->atom == netatoms[NET_WM_NAME])
			ewmh_client_list(c->xattr.root);
	}
	free(c);
}

// grab a MODKEY+key combo
void grab_key(Window root, KeySym key)
{
	KeyCode keycode = XKeysymToKeycode(display, key);
	XUngrabKey(display, keycode, AnyModifier, root);
	XGrabKey(display, keycode, config_modkey, root, True, GrabModeAsync, GrabModeAsync);
	XGrabKey(display, keycode, config_modkey|LockMask, root, True, GrabModeAsync, GrabModeAsync);
	if (NumlockMask)
	{
		XGrabKey(display, keycode, config_modkey|NumlockMask, root, True, GrabModeAsync, GrabModeAsync);
		XGrabKey(display, keycode, config_modkey|NumlockMask|LockMask, root, True, GrabModeAsync, GrabModeAsync);
	}
}

// window setup iterator
int setup_screen_window(int i, Window w, void *p)
{
	wincache *cache = calloc(sizeof(wincache), 1);
	winlist_append(windows, w, cache);
	client *c = window_client(w);
	if (c && c->manage)
	{
		winlist_append(windows_activated, c->window, NULL);
		client_review(c);
	}
	free(c);
	return 0;
}

// an X screen. may have multiple monitors, xinerama, etc
void setup_screen(int scr)
{
	int i; Window root = RootWindow(display, scr);

	unsigned long desktops = 1, desktop = 0;
	unsigned long workarea[4] = { 0, 0, DisplayWidth(display, scr), DisplayHeight(display, scr) };
	supporting = XCreateSimpleWindow(display, root, 0, 0, 1, 1, 0, 0, 0);
	unsigned long pid = getpid();

	// EWMH
	XChangeProperty(display, root, netatoms[NET_SUPPORTED], XA_ATOM, 32, PropModeReplace, (unsigned char*)netatoms, NETATOMS);

	// ewmh supporting wm
	XChangeProperty(display, root,       netatoms[NET_SUPPORTING_WM_CHECK], XA_WINDOW, 32, PropModeReplace, (unsigned char*)&supporting, 1);
	XChangeProperty(display, supporting, netatoms[NET_SUPPORTING_WM_CHECK], XA_WINDOW, 32, PropModeReplace, (unsigned char*)&supporting, 1);
	XChangeProperty(display, supporting, netatoms[NET_WM_NAME], XA_STRING,    8, PropModeReplace, (const unsigned char*)"GoomwWM", 6);
	XChangeProperty(display, supporting, netatoms[NET_WM_PID],  XA_CARDINAL, 32, PropModeReplace, (unsigned char*)&pid, 1);

	// one desktop. want more space? buy more monitors and use xinerama :)
	XChangeProperty(display, root, netatoms[NET_NUMBER_OF_DESKTOPS], XA_CARDINAL, 32, PropModeReplace, (unsigned char*)&desktops, 1);
	XChangeProperty(display, root, netatoms[NET_CURRENT_DESKTOP],    XA_CARDINAL, 32, PropModeReplace, (unsigned char*)&desktop, 1);
	XChangeProperty(display, root, netatoms[NET_DESKTOP_GEOMETRY],   XA_CARDINAL, 32, PropModeReplace, (unsigned char*)&workarea[2], 2);
	XChangeProperty(display, root, netatoms[NET_DESKTOP_VIEWPORT],   XA_CARDINAL, 32, PropModeReplace, (unsigned char*)&workarea, 2);
	XChangeProperty(display, root, netatoms[NET_WORKAREA],           XA_CARDINAL, 32, PropModeReplace, (unsigned char*)&workarea, 4);

	// MODKEY+
	const KeySym keys[] = {
		XK_Right, XK_Left, XK_Up, XK_Down, XK_Page_Up, XK_Page_Down, XK_Home, XK_End, XK_Insert, XK_Delete,
		XK_backslash, XK_bracketleft, XK_bracketright, XK_semicolon, XK_apostrophe, XK_Return,
		XK_F1, XK_F2, XK_F3, XK_F4, XK_F5, XK_F6, XK_F7, XK_F8, XK_F9, XK_t,
		XK_Tab, XK_grave, XK_Escape, XK_x, XK_equal, XK_i
	};

	// bind all MODKEY+ combos
	XUngrabKey(display, AnyKey, AnyModifier, root);
	for (i = 0; i < sizeof(keys)/sizeof(KeySym); i++) grab_key(root, keys[i]);

	if (config_key_1) grab_key(root, XK_1); if (config_key_2) grab_key(root, XK_2);
	if (config_key_3) grab_key(root, XK_3); if (config_key_4) grab_key(root, XK_4);
	if (config_key_5) grab_key(root, XK_5); if (config_key_6) grab_key(root, XK_6);
	if (config_key_7) grab_key(root, XK_7); if (config_key_8) grab_key(root, XK_8);
	if (config_key_9) grab_key(root, XK_9);

	// grab mouse buttons for click-to-focus. these get passed through to the windows
	// not binding on button4 which is usually wheel scroll
	XGrabButton(display, Button1, AnyModifier, root, True, ButtonPressMask, GrabModeSync, GrabModeSync, None, None);
	XGrabButton(display, Button2, AnyModifier, root, True, ButtonPressMask, GrabModeSync, GrabModeSync, None, None);
	XGrabButton(display, Button3, AnyModifier, root, True, ButtonPressMask, GrabModeSync, GrabModeSync, None, None);

	// become the window manager
	XSelectInput(display, root, SubstructureRedirectMask | SubstructureNotifyMask);

	// setup any existing windows
	winlist *wins = windows_in_play(root);
	winlist_iterate_up(wins, setup_screen_window, NULL);
	winlist_free(wins);
	// activate and focus top window
	free(window_active_client(root));
	ewmh_client_list(root);
}

int main(int argc, char *argv[])
{
	int i, j, scr; XEvent ev;
	signal(SIGCHLD, catch_exit);

	if(!(display = XOpenDisplay(0)))
	{
		fprintf(stderr, "cannot open display!\n");
		return 1;
	}

	// do this before setting error handler, so it fails if other wm in place
	XSelectInput(display, DefaultRootWindow(display), SubstructureRedirectMask);
	XSync(display, False); xerror = XSetErrorHandler(oops); XSync(display, False);

	// determine numlock mask so we can bind on keys with and without it
	XModifierKeymap *modmap = XGetModifierMapping(display);
	for (i = 0; i < 8; i++)
	{
		for (j = 0; j < (int)modmap->max_keypermod; j++)
			if (modmap->modifiermap[i*modmap->max_keypermod+j] == XKeysymToKeycode(display, XK_Num_Lock))
				NumlockMask = (1<<i);
	}
	XFreeModifiermap(modmap);

	// determine modkey
	config_modkey = MODKEY;
	char *modkeys = find_arg_str(argc, argv, "-modkey", NULL);
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
	// border colors
	config_border_focus = XGetColor(display, find_arg_str(argc, argv, "-focus", FOCUS));
	config_border_blur  = XGetColor(display, find_arg_str(argc, argv, "-blur",  BLUR));
	// border width in pixels
	config_border_width = MAX(0, find_arg_int(argc, argv, "-border", BORDER));
	// window flashing
	config_flash_on  = XGetColor(display, find_arg_str(argc, argv, "-flashon",  FLASHON));
	config_flash_off = XGetColor(display, find_arg_str(argc, argv, "-flashoff", FLASHOFF));
	config_flash_width = MAX(0, find_arg_int(argc, argv, "-flashpx", FLASHPX));
	config_flash_ms    = MAX(FLASHMS, find_arg_int(argc, argv, "-flashms", FLASHMS));
	// customizable keys
	config_switcher = find_arg_str(argc, argv, "-switcher", SWITCHER);
	config_launcher = find_arg_str(argc, argv, "-launcher", LAUNCHER);
	// app_find_or_start() keys
	config_key_1 = find_arg_str(argc, argv, "-1", NULL); config_key_2 = find_arg_str(argc, argv, "-2", NULL);
	config_key_3 = find_arg_str(argc, argv, "-3", NULL); config_key_4 = find_arg_str(argc, argv, "-4", NULL);
	config_key_5 = find_arg_str(argc, argv, "-5", NULL); config_key_6 = find_arg_str(argc, argv, "-6", NULL);
	config_key_7 = find_arg_str(argc, argv, "-7", NULL); config_key_8 = find_arg_str(argc, argv, "-8", NULL);
	config_key_9 = find_arg_str(argc, argv, "-9", NULL);

	// X atom values
	for (i = 0; i < ATOMS; i++) atoms[i] = XInternAtom(display, atom_names[i], False);
	for (i = 0; i < NETATOMS; i++) netatoms[i] = XInternAtom(display, netatom_names[i], False);

	// window tracking
	windows = winlist_new();
	windows_activated = winlist_new();

	// init on all screens/roots
	for (scr = 0; scr < ScreenCount(display); scr++) setup_screen(scr);

	// main event loop
	for(;;)
	{
		// block and wait for something
		XNextEvent(display, &ev);
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
#ifdef DEBUG
		else fprintf(stderr, "unhandled event %d: %x\n", ev.type, (unsigned int)ev.xany.window);
		catch_exit(0);
#endif
	}
	return EXIT_SUCCESS;
}
