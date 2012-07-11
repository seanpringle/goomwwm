% GOOMWWM(1)

# NAME

goomwwm \- Get out of my way, Window Manager!

# SYNOPSIS

**goomwwm** [ options ]

# DESCRIPTION

**goomwwm** is an X11 window manager implemented in C as a cleanroom software project. It manages windows in a minimal floating layout with normal mouse controls, while also providing flexible keyboard-driven controls for window switching, sizing, moving, tagging, and tiling. It is fast, lightweight, modeless, Xinerama-aware, and EWMH compatible wherever possible.

Keyboard window movement occurs on a 3x3 fullscreen grid. For example, a top-left aligned window moved to the right would reappear top-center, and if moved again, top-right.

Keyboard window sizing moves windows through four basic sizes that tile well: 1/9th of screen (1/3 width, 1/3 height), 1/4th, 4/9th, and fullscreen. Combined with EWMH horizontal and vertical maxmimization, plus some snap-to-edge and expand-to-fill-space controls, manual tiling is easy.

Windows are grouped by tags. Activating a tag raises all windows in the tag. A window may have multiple tags. EWMH panels, pagers, and taskbars see tags as desktops.

License: MIT/X11

# USAGE

See options below for custom key combinations. These are the defaults.

Mod4-Button1
:	(Mouse!) Move a window.

Mod4-Button3
:	(Mouse!) Resize a window.

Mod4-[F1-F9]
:	Set the current tag and raise all its windows. A window may be in multiple tags. For EWMH panel and pager integration tags roughly simulate desktops (always 9), but there will be differences in behavior.

Mod4-[1-9]
:	Do a case-insensitive keyword search for window by WM_CLASS and title. If found, raise and focus. If not found, try to execute the keyword and start the app. See **-1** through **-9** args.

Mod4-Tab
:	Switch between all windows by popup menu. This is partly based on **dmenu** but more tightly integrated with the window manager and centered on screen. Type text to filter the menu entries. Navigate with the arrow keys. Return to select.

Mod4-` (Grave/Backtick)
:	Switch between all windows in current tag by popup menu.

Mod4-c
:	Cycle through windows in the same tag and position as the active window.

Mod4-Escape
:	Close the active window.

Mod4-Page_Up
:	Grow the active window through four basic sizes that tile well together: 1/9th, 1/4th, 4/9th, or fullscreen.

Mod4-Page_Down
:	Shrink the active window through the same four sizes.

Mod4-h
:	Horizontally tile the active window and others with the same tag, position, and size.

Mod4-v
:	Vertically tile the active window and others with the same tag, position, and size.

Mod4-x
:	Run the launcher (by default: dmenu_run).

Mod4-a
:	Toggle _NET_WM_STATE_ABOVE for the active window. Corners will flash to acknowledge.

Mod4-b
:	Toggle _NET_WM_STATE_BELOW for the active window. Corners will flash to acknowledge.

Mod4-f
:	Toggle _NET_WM_STATE_FULLSCREEN for the active window. Corners will flash to acknowledge.

Mod4-d
:	Resize active window to match the window immediately underneath, in the same tag.

Mod4-Home
:	Toggle _NET_WM_STATE_MAXIMIXED_HORZ for the active window. Corners will flash to acknowledge.

Mod4-End
:	Toggle _NET_WM_STATE_MAXIMIXED_VERT for the active window. Corners will flash to acknowledge.

Mod4-Return
:	Expand active window to fill surrounding space without obscuring any fully visible window.

Mod4-Backspace
:	Contract active window to fill an underlying space without obscuring window that would then be fully visible.

Mod4-Insert
:	Toggle vertical move/resize lock for the active window.

Mod4-Delete
:	Toggle horizontal move/resize lock for the active window.

Mod4-Left
:	Move the active window left within a 3x3 grid.

Mod4-Right
:	Move the active window right within a 3x3 grid.

Mod4-Up
:	Move the active window up within a 3x3 grid.

Mod4-Down
:	Move the active window down within a 3x3 grid.

Mod4-u
:	Undo the last size/position change for the active window. Undo is 10 levels deep.

Mod4-j
:	Switch focus to the left of the active window.

Mod4-l
:	Switch focus to the right of the active window.

Mod4-i
:	Switch focus upward from the active window.

Mod4-k
:	Switch focus downward from the active window.

Mod4-t
:	Toggle the active window's membership of the current tag.

Mod4-m
:	Cycle tag forward.

Mod4-n
:	Cycle tag backward.

Mod4-w
:	Display active window title.

# OPTIONS

All key combinations use the same global modifier key (default: Mod4). If the modkey is changed with **-modkey** then all key combinations change to use it.

All options below that set a custom key implicitly combine it with the modifier key.

-1 -2 -3 -4 -5 -6 -7 -8 -9
:	Set a number key to a keyword to search for a window by WM_CLASS, applicaiton name, or title, and then raise and focus it. If a window is not found, the string supplied will be executed as a shell command to start the application.

	goomwwm -1 chromium -2 firefox -3 xterm

	Above, Mod4-1 would match the top-most Chromium window.

	Many applications politely set their WM_CLASS to a sane value (eg, Chromium uses Chromium, xterm uses XTerm) or append their name to their window titles, which nicely matches their binary names if we use case-insensitive string comparison. This allows us to use the one string to both search and start.

	Applications that are not so friendly can sometimes be wrapped in a shell script of the appropriate name in your $PATH somewhere.

-above
:	Set an X11 key name to toggle _NET_WM_STATE_ABOVE for the active window (default: XK_a).

	goomwwm -above a

-attention
:	Set the border color (X11 named color or hex #rrggbb) for an inactive window with _NET_WM_STATE_DEMANDS_ATTENTION (default: Red).

	goomwwm -attention Red

-auto
:	Search for an app at startup and autostart it if not found. Uses the same WM_CLASS/name/title matching rules as the -1 through -9 arguments (default: none).

	goomwwm -auto chromium

	Above, chromium will only be started if a chromium window does not already exist.


-below
:	Set an X11 key name to toggle _NET_WM_STATE_BELOW for the active window (default: XK_b).

	goomwwm -below b

-blur
:	Set the border color (X11 named color or hex #rrggbb) for unfocused windows (default: Dark Gray).

	goomwwm -blur "Dark Gray"

-border
:	Set the border width in pixels for all managed windows (default: 2).

	goomwwm -border 2

-close
:	Set an X11 key name to gracefully close the active window (default: XK_Escape).

	goomwwm -close Escape

-config
:	Parse extra options from a text file.

	goomwwm -config /path/to/config.txt

	The file format is any command line options without the leading hyphen. Comments and blank lines are acceptable.

		# a comment
		2 chromium
		2 konsole
		close Escape
		menufont mono-14

	If it exists, **$HOME/.goomwwmrc** is automatically parsed.

-cycle
:	Set an X11 key name to cycle windows in the same tag and position as the active window (default: XK_c).

	goomwwm -cycle c


-contract
:	Set an X11 key name to contract the active window to fill an underlying space without obscuring any other window that would then be fully visible (default: XK_Contract). Opposite of -expand.

	goomwwm -contract BackSpace

-down
:	Set an X11 key name to move the active window downward in a 3x3 grid (default: XK_Down).

	goomwwm -down Down

-duplicate
:	Set an X11 key name to resize the active window to match the window immediately underneath, in the same tag (default: XK_d).

	goomwwm -duplicate d

-exec
:	Execute a command at startup but only after goomwwm has started successfully (default: none). Useful for pre-lanching apps, but also see **-auto**.

	goomwwm -exec firefox

-expand
:	Set an X11 key name to expand the active window to fill adjacent space without obscuring any other fully visible window (default: XK_Return). Opposite of -contract.

	goomwwm -expand Return

-launch
:	Set an X11 key to run the application launcher (default: XK_x).

	goomwwm -launch x

-launcher
:	Set a custom application launcher to execute on **Mod4-x** (default: dmenu_run).

	goomwwm -launcher dmenu_run

-flashms
:	Set the duration in milliseconds of the window flash indicators (default: 500).

	goomwwm -flashms 500

-flashon
:	Set the color (X11 named color or hex #rrggbb) of the flash indicator when toggling _NET_WM_STATE_* on (default: Dark Green).

	goomwwm -flashon "Dark Green"

-flashoff
:	Set the color (X11 named color or hex #rrggbb) of the flash indicator when toggling _NET_WM_STATE_* off (default: Dark Red).

	goomwwm -flashon "Dark Red"

-flashpx
:	Set the size in pixels of window flash indicators (currently a colored square in each window corner) (default: 10).

	goomwwm -flashpx 10

-flashtitle
:	Wether to flash a window's title when changing focus or other modes (default: hide). Regardless of this setting, **Mod-w** always displays a window's title.

	goomwwm -flashtitle hide

	Valid modes:

	show
	:	Flash title bar centered on the window.

	hide
	:	Do no flash title bar.

-focus
:	Set the border color (X11 named color or hex #rrggbb) for the focused window (default: Royal Blue).

	goomwwm -focus "Royal Blue"

-focusdown
:	Set an X11 key name to switch focus downward form the active window (default: XK_k).

	goomwwm -focusdown k

-focusleft
:	Set an X11 key name to switch focus to left of the active window (default: XK_j).

	goomwwm -focusleft j

-focusmode
:	Control the window focus mode (default: click).

	goomwwm -focusmode click

	Valid settings are:

	click
	:	focus on mouse click.

	sloppy
	:	focus follows mouse

	sloppytag
	:	focus follows mouse within current tag.

-focusright
:	Set an X11 key name to switch focus to right of the active window (default: XK_l).

	goomwwm -focusright l

-focusup
:	Set an X11 key name to switch focus upward form the active window (default: XK_i).

	goomwwm -focusup i

-fullscreen
:	Set an X11 key name to toggle _NET_WM_STATE_FULLSCREEN for the active window (default: XK_f).

	goomwwm -fullscreen f

-grow
:	Set an X11 key name to increase the active window size (default: XK_Page_Up) through four basic sizes that tile well together: 1/9th, 1/4th, 4/9th, or fullscreen.

	goomwwm -grow Page_Up

-hlock
:	Set an X11 key name to toggle horizontal move/resize lock for the active window (default: XK_Delete).

	goomwwm -hlock Delete

-hmax
:	Set an X11 key name to toggle _NET_WM_STATE_MAXIMIXED_HORZ for the active window (default: XK_End).

	goomwwm -hmax End


-htile
:	Set an X11 key to horizontally tile the active window and others with the same tag, position, and size (default: XK_h). If no other window is found, a gap is created.

	goomwwm -htile h

-info
:	Set an X11 key to briefly display the active window's title (default: XK_w).

	goomwwm -info w

-left
:	Set an X11 key name to move the active window to the left in a 3x3 grid (default: XK_Left).

	goomwwm -left Left

-mapmode
:	Control the window initial map focus mode (default: steal).

	goomwwm -mapmode steal

	Valid settings are:

	steal
	:	new windows get focus.

	block
	:	new windows do not get focus.

-menubg
:	Set the background text color (X11 named color or hex #rrggbb) for the window-switcher menu (default: #f2f1f0).

	goomwwm -menubg "#f2f1f0"

-menubgalt
:	Set the alternate background text color (X11 named color or hex #rrggbb) for the window-switcher menu (default: #e9e8e7).

	goomwwm -menubgalt "#e9e8e7"

-menufg
:	Set the foreground text color (X11 named color or hex #rrggbb) for the window-switcher menu (default: #222222).

	goomwwm -menufg "#222222"

-menufont
:	Xft font name for use by the window-switcher menu (default: mono-14).

	goomwwm -menufont monospace-14:medium

-menuhlbg
:	Set the background text color (X11 named color or hex #rrggbb) for the highlighted item in the window-switcher menu (default: #005577).

	goomwwm -menufg "#005577"

-menuhlfg
:	Set the foreground text color (X11 named color or hex #rrggbb) for the highlighted item in the window-switcher menu (default: #ffffff).

	goomwwm -menufg "#ffffff"

-menulines
:	Maximum number of entries the window-switcher menu may show before scrolling (default: 25).

	goomwwm -menulines 25

-menuselect
:	Control how menu items are selected (default: return).

	goomwwm -menuselect return

	Valid settings are:

	return
	:	Menu stays open until item is selected with Enter/Return key. This is dmenu-like.

	modkeyup
	:	Menu stays open until item is selected by releasing the modkey. This is classic Alt-Tab window switching behavior.

-menuwidth
:	Set the width of the window-switcher menu as a percentage of the screen width if <= 100 (% symbol optional), or in pixels if >100 (default: 60%).

	goomwwm -menuwidth 60%
	goomwwm -menuwidth 800

-modkey
:	Change the modifier key mask to any combination of: shift,control,mod1,mod2,mod3,mod4,mod5 (default: mod4).

	goomwwm -modkey control,shift

-placement
:	Control the position of new windows (default: any).

	goomwwm -placement any

	Valid settings are:

	any
	:	Windows that specify or remember their placement are honored. Everything else is centered on the current monitor.

	center
	:	Windows are centered on the current monitor.

	pointer
	:	Windows are centered under the mouse pointer.

-quit
:	Set an X11 key name to exit the window manager (default: XK_Pause). This key must be **pressed twice** to take effect!

	goomwwm -quit Pause

-raisemode
:	Control the window raise mode (default: focus).

	goomwwm -raisemode focus

	Valid settings are:

	focus
	:	Window is raised on focus (default for -focusmode click).

	click
	:	Window is raised on Mod4-AnyButton click (default for -focusmode sloppy[tag]).

-right
:	Set an X11 key name to move the active window to the right in a 3x3 grid (default: XK_Right).

	goomwwm -right Right

-rule
:	Define a window control rule (default: none). This argument can be specified multiple times to apply multiple rules. If a window matches multiple rules only the last rule specified is used.

	goomwwm -rule "firefox tag9"
	goomwwm -rule "xfce4-notifyd ignore"
	goomwwm -rule "xterm left,maximize_vert,medium"

	Rules always have the format:

		pattern flag[...,flagN]

	The **pattern** is string matched against a window's WM_CLASS, application name, or title (in that order). All comparisons are case insensitive. When matching WM_CLASS or application name the pattern must match exactly. When matching a title the pattern need only appear somewhere in the text.

	Valid **flags** are:

	ignore
	:	Do not manage a window. Effectively makes a window behave as it the override_redirect flag is set.

	block
	:	Do not immediately focus the window when it first maps.

	tag1 tag2 tag3 tag4 tag5 tag6 tag7 tag8 tag9
	:	Apply tags to a window when it first opens. If the current tag is not in the list the window will not be raised or allowed to take focus.

	above below fullscreen maximize_horz maximize_vert sticky
	:	Apply respective _NET_WM_STATE_* to a window.

	left right top bottom
	:	Align a window with a screen edge. May be combined. Top trumps bottom. Left trumps right.

	small medium large cover expand contract
	:	Set a window's initial size (same increments as PageUp/Down). May be combined.

	hlock vlock
	:	Lock window horizontally or vertically.

-shrink
:	Set an X11 key name to decrease the active window size (default: XK_Page_Down) through four basic sizes that tile well together: 1/9th, 1/4th, 4/9th, or fullscreen.

	goomwwm -shrink Page_Down

-switch
:	Set an X11 key to start display window-switcher showing all open windows (default: XK_Tab).

	goomwwm -switch Tab

-switcher
:	Command to run an alternate window-switcher (default: built-in menu).

	goomwwm -switcher dswitch

-tag
:	Set an X11 key to toggle the active window's membership of the current tag (default: XK_t).

	goomwwm -tag t

-tswitch
:	Set an X11 key to start display window-switcher showing only windows in the current tag (default: XK_grave).

	goomwwm -tswitch grave

-tagnext
:	Set an X11 key to cycle tags forward (default: XK_m).

	goomwwm -tagnext m

-tagprev
:	Set an X11 key to cycle tags in reverse (default: XK_n).

	goomwwm -tagprev n

-titlebg
:	Set the background text color (X11 named color or hex #rrggbb) for window titles (default: #f2f1f0).

	goomwwm -titlebg "#f2f1f0"

-titlefg
:	Set the foreground text color (X11 named color or hex #rrggbb) for window titles (default: #222222).

	goomwwm -titlefg "#222222"

-titlefont
:	Xft font name for use by window titles (default: sans-14).

	goomwwm -titlefont sans-14:medium

-up
:	Set an X11 key name to move the active window upward in a 3x3 grid (default: XK_Up).

	goomwwm -up Up

-undo
:	Set an X11 key to undo the last size/position change for the active window (default: XK_u). Undo is 10 levels deep.

	goomwwm -undo u

-vlock
:	Set an X11 key name to toggle vertical move/resize lock for the active window (default: XK_Insert).

	goomwwm -vlock Insert

-vmax
:	Set an X11 key name to toggle _NET_WM_STATE_MAXIMIXED_VERT for the active window (default: XK_Home).

	goomwwm -vmax Home

-vtile
:	Set an X11 key to vertically tile the active window and other windows with the same tag, position, and size (default: XK_v). If no other window is found, a gap is created.

	goomwwm -vtile h

-warpmode
:	Control whether the mouse pointer warps to a focused window (default: never). This setting can make focusmode **sloppy** more cooperative when focus is changed by means other than the mouse.

	goomwwm -warpmode focus

	Valid settings are:

	never
	:	Pointer is never moved (default for -focusmode click).

	focus
	:	Pointer is warped to a newly focused window (default for -focusmode sloppy[tag]).


# OPTIONS (cli mode)

When run with **-cli** (command line interface) goomwwm may be used to dispatch commands to another running instance of goomwwm. Valid arguments are:

-restart
:	Restart a running goomwwm instance in place (without restarting X). Useful for reloading .goomwwmrc or upgrading to a new version.

	goomwwm -cli -restart

-exec
:	Switch to another window manager in place (without restarting X).

	goomwwm -cli -exec dwm

-quit
:	Exit goomwwm.

	goomwwm -cli -quit

# SEE ALSO

**dmenu** (1)

# AUTHOR

Sean Pringle <sean.pringle@gmail.com>