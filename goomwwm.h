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
#include <regex.h>
#include <X11/extensions/Xinerama.h>

typedef unsigned char bool;
typedef unsigned long long bitmap;

#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define READ 0
#define WRITE 1

#define NEAR(a,o,b) ((b) > (a)-(o) && (b) < (a)+(o))
#define SNAPTO(a,o,b,j) (NEAR((a),(o),(b)) ? (a): (b)+(j))
#define OVERLAP(a,b,c,d) (((a)==(c) && (b)==(d)) || MIN((a)+(b), (c)+(d)) - MAX((a), (c)) > 0)
#define INTERSECT(x,y,w,h,x1,y1,w1,h1) (OVERLAP((x),(w),(x1),(w1)) && OVERLAP((y),(h),(y1),(h1)))

#define WINLIST 32
#define MINWINDOW 16
#define UNDO 10
#define TOPLEFT 1
#define TOPRIGHT 2
#define BOTTOMLEFT 3
#define BOTTOMRIGHT 4
#define CENTERLEFT 5
#define CENTERRIGHT 6
#define CENTERTOP 7
#define CENTERBOTTOM 8
#define HORIZONTAL 1
#define VERTICAL 2
#define SNAPLEFT 1
#define SNAPRIGHT 2
#define SNAPUP 3
#define SNAPDOWN 4
#define FOCUSLEFT 1
#define FOCUSRIGHT 2
#define FOCUSUP 3
#define FOCUSDOWN 4
#define SWAPLEFT 1
#define SWAPRIGHT 2
#define SWAPUP 3
#define SWAPDOWN 4
#define CLIENTSTATE 7

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
#define RULE_RAISE 1LL<<32
#define RULE_LOWER 1LL<<33
#define RULE_SNAPLEFT 1LL<<34
#define RULE_SNAPRIGHT 1LL<<35
#define RULE_SNAPUP 1LL<<36
#define RULE_SNAPDOWN 1LL<<37
#define RULE_SIZE 1LL<<38
#define RULE_DUPLICATE 1LL<<39
#define RULE_MINIMIZE 1LL<<40
#define RULE_RESTORE 1LL<<41
#define RULE_MONITOR1 1LL<<42
#define RULE_MONITOR2 1LL<<43
#define RULE_MONITOR3 1LL<<44
#define RULE_ONCE 1LL<<45
#define RULE_HTILE 1LL<<46
#define RULE_HUNTILE 1LL<<47
#define RULE_VTILE 1LL<<48
#define RULE_VUNTILE 1LL<<49
#define RULE_RESET 1LL<<50
#define RULE_AUTOMINI 1LL<<51
#define RULE_REPLACE 1LL<<52
#define RULE_CENTER 1LL<<53
#define RULE_POINTER 1LL<<54

#define RULESDEF 0
#define RULESRESET 1

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
#define MENUBC "#c0c0c0"
#define TITLEXFTFONT "sans-14"
#define TITLEFG "#222222"
#define TITLEBG "#f2f1f0"
#define TITLEBC "#c0c0c0"
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
#define RESIZEINC 1
#define NORESIZEINC 0
#define SMARTRESIZEINC 2
#define LARGELEFT 1
#define LARGERIGHT 2

#define SMARTRESIZEINC_IGNORE "^(xterm|urxvt)$"

#define winlist_ascend(l,i,w) for ((i) = 0; (i) < (l)->len && (((w) = (l)->array[i]) || 1); (i)++)
#define winlist_descend(l,i,w) for ((i) = (l)->len-1; (i) >= 0 && (((w) = (l)->array[i]) || 1); (i)--)

#define clients_ascend(l,i,w,c) winlist_ascend(l,i,w) if (((c) = client_create(w)))
#define clients_descend(l,i,w,c) winlist_descend(l,i,w) if (((c) = client_create(w)))

#define managed_ascend(i,w,c) clients_ascend(windows_in_play(),i,w,c) if ((c)->manage && (c)->visible)
#define managed_descend(i,w,c) clients_descend(windows_in_play(),i,w,c) if ((c)->manage && (c)->visible)

#define tag_ascend(i,w,c,t) managed_ascend(i, w, c) if (!(c)->cache->tags || !t || (c)->cache->tags & (t))
#define tag_descend(i,w,c,t) managed_descend(i, w, c) if (!(c)->cache->tags || !t || (c)->cache->tags & (t))

// window lists
typedef struct {
	Window *array; // actual window ids
	void **data;   // an associated struct
	short len;
} winlist;

// usable space on a monitor
typedef struct {
	short x, y, w, h, l, r, t, b;
} workarea;

// snapshot a window's size/pos and EWMH state
typedef struct _winundo {
	short x, y, w, h, sx, sy, sw, sh, states;
	Atom state[CLIENTSTATE];
	struct _winundo *next;
} winundo;

// track general window stuff
// every window we know about gets one of these, even if it's empty
typedef struct {
	bool have_closed;  // true when we've previously politely sent a close request
	bool last_corner;  // the last screen corner, used to make corner seem sticky during resizing
	bool hlock, vlock; // horizontal and vertical size/position locks
	bool has_mapped;   // true when a client has mapped previously. used to avoid applying rules
	unsigned int tags; // desktop tags
	winundo *ewmh;     // undo size/pos for EWMH FULLSCREEN/MAXIMIZE_HORZ/MAXIMIZE_VERT toggles
	winundo *undo;     // general size/pos undo LIFO linked list
	Window frame;      // titlebar & border, but NOT reparented!
	bool is_ours;      // set for any windows goomwwm creates
} wincache;

// rule for controlling window size/pos/behaviour
typedef struct _rule {
	char *pattern; // POSIX regex pattern to match on class/name/title
	regex_t re;    // precompiled regex
	bitmap flags;  // RULE_* flags
	short w, h;    // manually specified width/height
	bool w_is_pct, h_is_pct; // true if w/h is a percentage of screen size
	struct _rule *next;
} winrule;

// all global rules. this is separate from rule sets!
winrule *config_rules = NULL;

// a set of rules to execute in order, like a mini script.
// this is separate from global rules!
typedef struct _ruleset {
	char *name;     // any name, for disply in the popup menu
	winrule *rules; // linked list of rules in reverse-definition order
	struct _ruleset *next;
} winruleset;

// all defined rulesets
winruleset *config_rulesets = NULL;

// for converting rule strings to bit flags
typedef struct {
	const char *name;
	bitmap flag;
} winrulemap;

winrulemap rulemap[] = {
	{ "tag1", TAG1 },
	{ "tag2", TAG2 },
	{ "tag3", TAG3 },
	{ "tag4", TAG4 },
	{ "tag5", TAG5 },
	{ "tag6", TAG6 },
	{ "tag7", TAG7 },
	{ "tag8", TAG8 },
	{ "tag9", TAG9 },
	{ "ignore", RULE_IGNORE },
	{ "above", RULE_ABOVE },
	{ "sticky", RULE_STICKY },
	{ "below", RULE_BELOW },
	{ "fullscreen", RULE_FULLSCREEN },
	{ "maximize_horz", RULE_MAXHORZ },
	{ "maximize_vert", RULE_MAXVERT },
	{ "top",    RULE_TOP },
	{ "bottom", RULE_BOTTOM },
	{ "left",   RULE_LEFT },
	{ "right",  RULE_RIGHT },
	{ "center", RULE_CENTER },
	{ "pointer", RULE_POINTER },
	{ "small",  RULE_SMALL },
	{ "medium", RULE_MEDIUM },
	{ "large",  RULE_LARGE },
	{ "cover", RULE_COVER },
	{ "replace", RULE_REPLACE },
	{ "steal", RULE_STEAL },
	{ "block", RULE_BLOCK },
	{ "hlock", RULE_HLOCK },
	{ "vlock", RULE_VLOCK },
	{ "expand", RULE_EXPAND },
	{ "contract", RULE_CONTRACT },
	{ "skip_taskbar", RULE_SKIPTBAR },
	{ "skip_pager", RULE_SKIPPAGE },
	{ "raise", RULE_RAISE },
	{ "lower", RULE_LOWER },
	{ "snap_left", RULE_SNAPLEFT },
	{ "snap_right", RULE_SNAPRIGHT },
	{ "snap_up", RULE_SNAPUP },
	{ "snap_down", RULE_SNAPDOWN },
	{ "duplicate", RULE_DUPLICATE },
	{ "minimize", RULE_MINIMIZE },
	{ "restore", RULE_RESTORE },
	{ "monitor1", RULE_MONITOR1 },
	{ "monitor2", RULE_MONITOR2 },
	{ "monitor3", RULE_MONITOR3 },
	{ "once", RULE_ONCE },
	{ "htile", RULE_HTILE },
	{ "vtile", RULE_VTILE },
	{ "huntile", RULE_HUNTILE },
	{ "vuntile", RULE_VUNTILE },
	{ "reset", RULE_RESET },
	{ "minimize_auto", RULE_AUTOMINI },
};

// a placeholder
char *empty = "";

// collect and store data on a window
typedef struct {
	Window window;           // window's id
	Window trans;            // our transient_for
	XWindowAttributes xattr; // copy of cache_xattr data
	XSizeHints xsize;        // only loaded after client_extended_data()
	short x, y, w, h;        // size/pos pulled from xattr
	short sx, sy;            // pos relative to monitor
	short states;            // number of EWMH states set
	short initial_state;     // pulled from wm hints
	short border_width;      // pulled from xwindowattributes
	// general flags
	bool manage, visible, input, focus, active, minimized, shaded, decorate, urgent;
	bool is_full, is_left, is_top, is_right, is_bottom, is_xcenter, is_ycenter;
	bool is_maxh, is_maxv, is_described, is_extended, is_ruled;
	// descriptive buffers loaded after client_descriptive_data()
	char *title, *class, *name;
	// EWMH states and type
	Atom state[CLIENTSTATE], type;
	workarea monitor; // monitor holding the window, with strut padding detected
	wincache *cache;  // a persistent cache for this window (clients are freed each event)
	winrule *rule;    // loaded after client_rule
} client;

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
	short done, max_lines, num_lines, input_size, line_height;
	short current, width, height, horz_pad, vert_pad, offset;
	char *input, *selected, *manual;
	XIM xim;
	XIC xic;
};

// config settings
unsigned int config_modkey, config_prefix_mode, config_border_focus,
	config_border_blur, config_border_attention, config_flash_on,
	config_flash_off, config_warp_mode, config_flash_title,
	config_border_width, config_flash_width, config_flash_ms,
	config_map_mode, config_menu_select, config_menu_width,
	config_menu_lines, config_focus_mode, config_raise_mode,
	config_window_placement, config_only_auto, config_resize_inc;

char *config_menu_font, *config_menu_fg, *config_menu_bg,
	*config_menu_hlfg, *config_menu_hlbg, *config_menu_bgalt,
	*config_title_font, *config_title_fg, *config_title_bg,
	*config_menu_bc, *config_title_bc, *config_resizeinc_ignore;

char *config_switcher, *config_launcher, *config_apps_patterns[10];
KeySym config_apps_keysyms[] = { XK_0, XK_1, XK_2, XK_3, XK_4, XK_5, XK_6, XK_7, XK_8, XK_9, 0 };
KeySym config_tags_keysyms[] = { XK_F1, XK_F2, XK_F3, XK_F4, XK_F5, XK_F6, XK_F7, XK_F8, XK_F9, 0 };

#define MAXMODCODES 16
unsigned int config_modkeycodes[MAXMODCODES+1];

#define KEY_ENUM(a,b,c,d) a
#define KEY_KSYM(a,b,c,d) [a] = c
#define KEY_KMOD(a,b,c,d) [a] = b
#define KEY_CARG(a,b,c,d) #d

// default keybindings
#define KEYLIST(X) \
	X(KEY_RIGHT,              0, XK_Right,      -right     ),\
	X(KEY_LEFT,               0, XK_Left,       -left      ),\
	X(KEY_UP,                 0, XK_Up,         -up        ),\
	X(KEY_DOWN,               0, XK_Down,       -down      ),\
	X(KEY_SNAPRIGHT,  ShiftMask, XK_Right,      -snapright ),\
	X(KEY_SNAPLEFT,   ShiftMask, XK_Left,       -snapleft  ),\
	X(KEY_SNAPUP,     ShiftMask, XK_Up,         -snapup    ),\
	X(KEY_SNAPDOWN,   ShiftMask, XK_Down,       -snapdown  ),\
	X(KEY_FOCUSRIGHT,         0, XK_l,          -focusright),\
	X(KEY_FOCUSLEFT,          0, XK_j,          -focusleft ),\
	X(KEY_FOCUSUP,            0, XK_i,          -focusup   ),\
	X(KEY_FOCUSDOWN,          0, XK_k,          -focusdown ),\
	X(KEY_SWAPRIGHT,  ShiftMask, XK_l,          -swapright ),\
	X(KEY_SWAPLEFT,   ShiftMask, XK_j,          -swapleft  ),\
	X(KEY_SWAPUP,     ShiftMask, XK_i,          -swapup    ),\
	X(KEY_SWAPDOWN,   ShiftMask, XK_k,          -swapdown  ),\
	X(KEY_SHRINK,             0, XK_Page_Down,  -shrink    ),\
	X(KEY_GROW,               0, XK_Page_Up,    -grow      ),\
	X(KEY_DEC,        ShiftMask, XK_Page_Down,  -decrease  ),\
	X(KEY_INC,        ShiftMask, XK_Page_Up,    -increase  ),\
	X(KEY_FULLSCREEN,         0, XK_f,          -fullscreen),\
	X(KEY_ABOVE,              0, XK_a,          -above     ),\
	X(KEY_BELOW,              0, XK_b,          -below     ),\
	X(KEY_STICKY,             0, XK_s,          -sticky    ),\
	X(KEY_VMAX,               0, XK_Home,       -vmax      ),\
	X(KEY_HMAX,               0, XK_End,        -hmax      ),\
	X(KEY_EXPAND,             0, XK_Return,     -expand    ),\
	X(KEY_CONTRACT,           0, XK_BackSpace,  -contract  ),\
	X(KEY_VLOCK,              0, XK_Insert,     -vlock     ),\
	X(KEY_HLOCK,              0, XK_Delete,     -hlock     ),\
	X(KEY_TAG,                0, XK_t,          -tag       ),\
	X(KEY_SWITCH,             0, XK_Tab,        -switch    ),\
	X(KEY_TSWITCH,            0, XK_grave,      -tswitch   ),\
	X(KEY_CYCLE,              0, XK_c,          -cycle     ),\
	X(KEY_CLOSE,              0, XK_Escape,     -close     ),\
	X(KEY_TAGCLOSE,   ShiftMask, XK_Escape,     -tagclose  ),\
	X(KEY_HTILE,              0, XK_h,          -htile     ),\
	X(KEY_VTILE,              0, XK_v,          -vtile     ),\
	X(KEY_HUNTILE,    ShiftMask, XK_h,          -huntile   ),\
	X(KEY_VUNTILE,    ShiftMask, XK_v,          -vuntile   ),\
	X(KEY_UNDO,               0, XK_u,          -undo      ),\
	X(KEY_TAGNEXT,            0, XK_m,          -tagnext   ),\
	X(KEY_TAGPREV,            0, XK_n,          -tagprev   ),\
	X(KEY_DUPLICATE,          0, XK_d,          -duplicate ),\
	X(KEY_INFO,               0, XK_w,          -info      ),\
	X(KEY_QUIT,               0, XK_Pause,      -quit      ),\
	X(KEY_PREFIX,             0, XK_VoidSymbol, -prefix    ),\
	X(KEY_MINIMIZE,           0, XK_slash,      -minimize  ),\
	X(KEY_RULE,               0, XK_comma,      -runrule   ),\
	X(KEY_RULESET,            0, XK_period,     -runruleset),\
	X(KEY_TAGONLY,            0, XK_o,          -only      ),\
	X(KEY_LARGELEFT,          0, XK_bracketleft,  -largeleft ),\
	X(KEY_LARGERIGHT,         0, XK_bracketright, -largeright),\
	X(KEY_LAUNCH,             0, XK_x,          -launch    )

enum { KEYLIST(KEY_ENUM) };
KeySym keymap[] = { KEYLIST(KEY_KSYM), 0 };
unsigned int keymodmap[] = { KEYLIST(KEY_KMOD), 0 };
char *keyargs[] = { KEYLIST(KEY_CARG), NULL };

unsigned int NumlockMask = 0;
Display *display; Screen *screen; Window root; int screen_id;

// mouse move/resize controls
// see ButtonPress,MotionNotify
bool mouse_dragging = 0;
XButtonEvent mouse_button;
XWindowAttributes mouse_attr;
bool quit_pressed_once = 0;
bool prefix_mode_active = 0;
Cursor prefix_cursor;
Window supporting;

// tracking windows
winlist *windows, *windows_activated, *windows_minimized, *windows_shaded;
unsigned int current_tag = TAG1;

// caches used to reduce X server round trips
winlist *cache_client;
winlist *cache_xattr;
winlist *cache_inplay;

workarea cache_monitor[6];

static int (*xerror)(Display *, XErrorEvent *);

typedef struct {
	unsigned long flags, functions, decorations;
} motif_hints;

#define ATOM_ENUM(x) x
#define ATOM_CHAR(x) #x

#define GENERAL_ATOMS(X) \
	X(_MOTIF_WM_HINTS),\
	X(WM_DELETE_WINDOW),\
	X(WM_STATE),\
	X(WM_TAKE_FOCUS),\
	X(WM_NAME),\
	X(WM_CLASS),\
	X(WM_WINDOW_ROLE),\
	X(WM_PROTOCOLS)

enum { GENERAL_ATOMS(ATOM_ENUM), ATOMS };
const char *atom_names[] = { GENERAL_ATOMS(ATOM_CHAR) };
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
	X(_NET_WM_WINDOW_TYPE_DROPDOWN_MENU),\
	X(_NET_WM_WINDOW_TYPE_POPUP_MENU),\
	X(_NET_WM_WINDOW_TYPE_TOOLTIP),\
	X(_NET_WM_WINDOW_TYPE_NOTIFICATION),\
	X(_NET_WM_WINDOW_TYPE_COMBO),\
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
	X(_NET_FRAME_EXTENTS),\
	X(_NET_REQUEST_FRAME_EXTENTS),\
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
	X(GOOMWWM_RULESET),\
	X(GOOMWWM_RULE),\
	X(GOOMWWM_NOTICE),\
	X(GOOMWWM_FIND_OR_START),\
	X(GOOMWWM_RESTART)

enum { GOOMWWM_ATOMS(ATOM_ENUM), GATOMS };
const char *gatom_names[] = { GOOMWWM_ATOMS(ATOM_CHAR) };
Atom gatoms[GATOMS];
