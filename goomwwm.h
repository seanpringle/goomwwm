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

#include <X11/X.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xmd.h>
#include <X11/Xutil.h>
#include <X11/Xproto.h>
#include <X11/keysym.h>
#include <X11/cursorfont.h>
#include <X11/XKBlib.h>
#include <X11/Xft/Xft.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <ctype.h>
#include <math.h>
#include <sys/time.h>
#include <signal.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <X11/extensions/Xinerama.h>

// window lists
typedef struct {
	Window *array;
	void **data;
	int len;
} winlist;

#define WINLIST 32

#define winlist_ascend(l,i,w) for ((i) = 0; (i) < (l)->len && (((w) = (l)->array[i]) || 1); (i)++)
#define winlist_descend(l,i,w) for ((i) = (l)->len-1; (i) >= 0 && (((w) = (l)->array[i]) || 1); (i)--)

#define clients_ascend(l,i,w,c) winlist_ascend(l,i,w) if (((c) = client_create(w)))
#define clients_descend(l,i,w,c) winlist_descend(l,i,w) if (((c) = client_create(w)))

#define managed_ascend(r,i,w,c) clients_ascend(windows_in_play(r),i,w,c) if ((c)->manage && (c)->visible)
#define managed_descend(r,i,w,c) clients_descend(windows_in_play(r),i,w,c) if ((c)->manage && (c)->visible)

#define tag_ascend(r,i,w,c,t) managed_ascend(r, i, w, c) if (!(c)->cache->tags || !t || (c)->cache->tags & (t))
#define tag_descend(r,i,w,c,t) managed_descend(r, i, w, c) if (!(c)->cache->tags || !t || (c)->cache->tags & (t))

// usable space on a monitor
typedef struct {
	int x, y, w, h;
	int l, r, t, b;
} workarea;

#define UNDO 10
#define TOPLEFT 1
#define TOPRIGHT 2
#define BOTTOMLEFT 3
#define BOTTOMRIGHT 4
#define HORIZONTAL 1
#define VERTICAL 2
#define FOCUSLEFT 1
#define FOCUSRIGHT 2
#define FOCUSUP 3
#define FOCUSDOWN 4
#define CLIENTTITLE 100
#define CLIENTCLASS 50
#define CLIENTNAME 50
#define CLIENTSTATE 10

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
	double mr_time;
	int have_closed;
	int last_corner;
	unsigned int tags;
	int undo_levels;
	winundo undo[UNDO];
	int hlock, vlock;
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

// these must be above tag bits
#define RULE_IGNORE 1<<9
#define RULE_FULLSCREEN 1<<10
#define RULE_ABOVE 1<<11
#define RULE_STICKY 1<<12
#define RULE_BELOW 1<<13
#define RULE_MAXHORZ 1<<14
#define RULE_MAXVERT 1<<15
#define RULE_TOP 1<<16
#define RULE_BOTTOM 1<<17
#define RULE_LEFT 1<<18
#define RULE_RIGHT 1<<19
#define RULE_SMALL 1<<20
#define RULE_MEDIUM 1<<21
#define RULE_LARGE 1<<22
#define RULE_COVER 1<<23
#define RULE_STEAL 1<<24
#define RULE_BLOCK 1<<25
#define RULE_HLOCK 1<<26
#define RULE_VLOCK 1<<27
#define RULE_EXPAND 1<<28
#define RULE_CONTRACT 1<<29
#define RULE_SKIPTBAR 1<<30
#define RULE_SKIPPAGE 1<<31
#define RULE_RAISE 1L<<32
#define RULE_LOWER 1L<<33

#define RULEPATTERN CLIENTCLASS

typedef struct _rule {
	char pattern[RULEPATTERN];
	unsigned long long flags;
	struct _rule *next;
} winrule;

winrule *config_rules = NULL;

typedef struct {
	const char *name;
	unsigned long long flag;
} winrulemap;

// a managable window
typedef struct {
	Window window, trans;
	XWindowAttributes xattr;
	XSizeHints xsize;
	int manage, visible, input, focus, active, initial_state,
		x, y, w, h, sx, sy, sw, sh,
		is_full, is_left, is_top, is_right, is_bottom,
		is_xcenter, is_ycenter, is_maxh, is_maxv, states,
		is_described, is_extended;
	char title[CLIENTTITLE], class[CLIENTCLASS], name[CLIENTNAME];
	Atom state[CLIENTSTATE], type;
	workarea monitor;
	wincache *cache;
	winrule *rule; int is_ruled;
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
#define FLASHPX 10
#define FLASHMS 500
#define FLASHMSTITLE 2000
#define FLASHTITLE 1
#define FLASHTITLEDEF 0
#define SAYMS 2000
#define MODKEY Mod4Mask
#define MENUXFTFONT "mono-14"
#define MENUWIDTH 50
#define MENULINES 25
#define MENUFG "#222222"
#define MENUBG "#f2f1f0"
#define MENUBGALT "#e9e8e7"
#define MENUHLFG "#ffffff"
#define MENUHLBG "#005577"
#define TITLEXFTFONT "sans-14"
#define TITLEFG "#222222"
#define TITLEBG "#f2f1f0"
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
#define MAPSTEAL 1
#define MAPBLOCK 2
#define WARPFOCUS 1
#define WARPNEVER 0
#define PLACEANY 1
#define PLACECENTER 2
#define PLACEPOINTER 3
#define FLASH 1
#define NOFLASH 0
#define MENURETURN 1
#define MENUMODUP 2
#define PREFIX 1
#define NOPREFIX 0

unsigned int config_modkey, config_ignore_modkeys, config_prefix_mode,
	config_border_focus, config_border_blur, config_border_attention,
	config_flash_on, config_flash_off, config_warp_mode, config_flash_title,
	config_border_width, config_flash_width, config_flash_ms, config_map_mode, config_menu_select,
	config_menu_width, config_menu_lines, config_focus_mode, config_raise_mode, config_window_placement;

char *config_menu_font, *config_menu_fg, *config_menu_bg, *config_menu_hlfg, *config_menu_hlbg, *config_menu_bgalt,
	*config_title_font, *config_title_fg, *config_title_bg;

char *config_switcher, *config_launcher, *config_apps_patterns[10];
KeySym config_apps_keysyms[] = { XK_0, XK_1, XK_2, XK_3, XK_4, XK_5, XK_6, XK_7, XK_8, XK_9, 0 };
KeySym config_tags_keysyms[] = { XK_F1, XK_F2, XK_F3, XK_F4, XK_F5, XK_F6, XK_F7, XK_F8, XK_F9, 0 };

#define MAXMODCODES 16
unsigned int config_modkeycodes[MAXMODCODES+1];

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
	X(KEY_VLOCK, XK_Insert, -vlock),\
	X(KEY_HLOCK, XK_Delete, -hlock),\
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
	X(KEY_DUPLICATE, XK_d, -duplicate),\
	X(KEY_INFO, XK_w, -info),\
	X(KEY_QUIT, XK_Pause, -quit),\
	X(KEY_PREFIX, XK_VoidSymbol, -prefix),\
	X(KEY_LAUNCH, XK_x, -launch)

enum { KEYLIST(KEY_ENUM) };
KeySym keymap[] = { KEYLIST(KEY_KSYM), 0 };
char *keyargs[] = { KEYLIST(KEY_CARG), NULL };

unsigned int NumlockMask = 0;
Display *display;

// mouse move/resize controls
// see ButtonPress,MotionNotify
int mouse_dragging = 0;
XButtonEvent mouse_button;
XWindowAttributes mouse_attr;
int quit_pressed_once = 0;
int prefix_mode_active = 0;
Cursor prefix_cursor;

// tracking windows
winlist *windows, *windows_activated;
unsigned int current_tag = TAG1;

// caches used to reduce X server round trips
winlist *cache_client;
winlist *cache_xattr;
winlist *cache_inplay;

workarea cache_monitor[10];

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

#define GOOMWWM_ATOMS(X) \
	X(GOOMWWM_LOG),\
	X(GOOMWWM_MESSAGE),\
	X(GOOMWWM_QUIT),\
	X(GOOMWWM_RESTART)

enum { GOOMWWM_ATOMS(ATOM_ENUM), GATOMS };
const char *gatom_names[] = { GOOMWWM_ATOMS(ATOM_CHAR) };
Atom gatoms[GATOMS];

// built-in filterable popup menu list
struct localmenu {
	Window window;
	GC gc;
	Pixmap canvas;
	XftFont *font;
	XftColor *color;
	XftDraw *draw;
	XftColor fg, bg, hlfg, hlbg, bgalt;
	unsigned long xbg;
	char **lines, **filtered;
	int done, max_lines, num_lines, input_size, line_height;
	int current, width, height, horz_pad, vert_pad, offset;
	char *input, *selected, *manual;
	XIM xim;
	XIC xic;
};

#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define READ 0
#define WRITE 1

#define NEAR(a,o,b) ((b) > (a)-(o) && (b) < (a)+(o))
#define OVERLAP(a,b,c,d) (((a)==(c) && (b)==(d)) || MIN((a)+(b), (c)+(d)) - MAX((a), (c)) > 0)
#define INTERSECT(x,y,w,h,x1,y1,w1,h1) (OVERLAP((x),(w),(x1),(w1)) && OVERLAP((y),(h),(y1),(h1)))

#define MINWINDOW 16