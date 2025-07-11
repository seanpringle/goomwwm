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

Below, **Mod** refers to the global modifier key which is **Mod4** by default. Usually Mod4 is mapped to the left Windows key. To change it, see **-modkey**.

Mod-Button1
:	(Mouse!) Move a window.

Mod-Button3
:	(Mouse!) Resize a window.

Mod-[F1-F9]
:	Set the current tag and raise all its windows. A window may be in multiple tags. For EWMH panel and pager integration tags roughly simulate desktops (always 9), but there will be differences in behavior.

Mod-Shift-[F1-F9]
:	Toggle active window's tags without switching current tag.

Mod-[1-9]
:	Do a case-insensitive keyword search for window by WM_CLASS and title. If found, raise and focus. If not found, try to execute the keyword and start the app. See **-1** through **-9** args.

Mod-Shift-[1-9]
:	Force execution of keyword used in **Mod-[1-9]** even if a copy of the window already exists.

Mod-Tab
:	Switch between all windows by popup menu. This is partly based on **dmenu** but more tightly integrated with the window manager and centered on screen. Type text to filter the menu entries. Navigate with the arrow keys. Return to select.

Mod-` (Grave/Backtick)
:	Switch between all windows in current tag by popup menu.

Mod-c
:	Cycle through windows in the same tag and position as the active window.

Mod-Escape
:	Close the active window.

Mod-PageUp
:	Grow the active window through four basic sizes that tile well together: 1/9th, 1/4th, 4/9th, or fullscreen.

Mod-PageDown
:	Shrink the active window through the same four sizes.

Mod-Shift-PageUp
:	Increase size of the active window.

Mod-Shift-PageDown
:	Decrease size of the active window.

Mod-h
:	Horizontally tile the active window and others with the same tag, position, and size.

Mod-v
:	Vertically tile the active window and others with the same tag, position, and size.

Mod-Shift-h
:	Opposite of Mod-h. Horizontally un-tile.

Mod-Shift-v
:	Opposite of Mod-v. Vertically un-tile.

Mod-x
:	Run the launcher (by default: dmenu_run).

Mod-a
:	Toggle _NET_WM_STATE_ABOVE for the active window. Corners will flash to acknowledge.

Mod-b
:	Toggle _NET_WM_STATE_BELOW for the active window. Corners will flash to acknowledge.

Mod-f
:	Toggle _NET_WM_STATE_FULLSCREEN for the active window. Corners will flash to acknowledge.

Mod-d
:	Resize active window to match the window immediately underneath, in the same tag.

Mod-Home
:	Toggle _NET_WM_STATE_MAXIMIXED_HORZ for the active window. Corners will flash to acknowledge.

Mod-End
:	Toggle _NET_WM_STATE_MAXIMIXED_VERT for the active window. Corners will flash to acknowledge.

Mod-Return
:	Expand active window to fill surrounding space without obscuring any fully visible window.

Mod-Backspace
:	Contract active window to fill an underlying space without obscuring window that would then be fully visible.

Mod-Insert
:	Toggle vertical move/resize lock for the active window.

Mod-Delete
:	Toggle horizontal move/resize lock for the active window.

Mod-Left
:	Move the active window left within a 3x3 grid.

Mod-Right
:	Move the active window right within a 3x3 grid.

Mod-Up
:	Move the active window up within a 3x3 grid.

Mod-Down
:	Move the active window down within a 3x3 grid.

Mod-Shift-Left (Left/Right/Up/Down)
:	Snap the active window to the nearest border, by direction.

Mod-u
:	Undo the last size/position change for the active window. Undo is 10 levels deep.

Mod-i
:	Switch focus upward from the active window.

Mod-j
:	Switch focus to the left of the active window.

Mod-k
:	Switch focus downward from the active window.

Mod-l
:	Switch focus to the right of the active window.

Mod-Shift-i (i/j/k/l)
:	Swap the active window position with another window by direction.

Mod-t
:	Toggle the active window's membership of the current tag.

Mod-m
:	Cycle tag forward.

Mod-n
:	Cycle tag backward.

Mod-w
:	Display active window title.

Mod-, (comma)
:	Reapply active window rule.

Mod-. (period)
:	Switch between defined rule sets.

Mod-/ (slash)
:	Minimize a window.

Mod-[ (left square brancket)
:	Move and resize a window to cover the left 2/3 of a monitor.

Mod-] (right square brancket)
:	Move and resize a window to cover the right 2/3 of a monitor.

Mod-o
:	Show only windows in the current tag. Hide everything else.

Mod-Pause  (press twice)
:	Quit goomwwm.

# OPTIONS

All key combinations use the same global modifier key by default, which is **Mod4** (usually Win/Meta). If the default modifier is changed with **-modkey** then all key combinations that do not specify their own custom modifiers will change to use the new modifier automatically.

All options below that set a custom key therefore implicitly combine it with the default modifier key. For example, the following both mean **Mod4-a**:

	goomwwm -above a
	goomwwm -above mod4-a

Any combiation of **shift**, **control**, **mod1** (usually Alt), **mod2**, **mod3**, **mod4** (usually Win/Meta), **mod5** (sometimes AltGr) may be supplied for any key combination:

	goomwwm -above control-shift-a

To explicitly bind a key without any modifier, not even the default, use **nomod**:

	goomwwm -above nomod-f12

Note that this would capture F12 globally, making it unusable for anything else. Use *nomod* with care.

-1 -2 -3 -4 -5 -6 -7 -8 -9
:	Set a number key to a keyword to search for a window by WM_CLASS, application name, or title, and then raise and focus it. If a window is not found, the string supplied will be executed as a shell command to start the application.

	goomwwm -1 chromium -2 firefox -3 xterm

	Above, Mod-1 would match the top-most Chromium window.

	Many applications politely set their WM_CLASS to a sane value (eg, Chromium uses Chromium, xterm uses XTerm) or append their name to their window titles, which nicely matches their binary names if we use case-insensitive string comparison. This allows us to use the one string to both search and start.

	Applications that are not so friendly can sometimes be wrapped in a shell script of the appropriate name in your $PATH somewhere.

	Sometimes it can be useful to limit the match to WM_CLASS or name (eg, when a browser window title includes another app's name causing a false positive). Simply use **class:** or **name:** prefixes:

	goomwwm -1 class:chromium

-above
:	Set an X11 key name to toggle _NET_WM_STATE_ABOVE for the active window (default: XK_a).

	goomwwm -above a

-appkeys
:	Specify the keys to use for app search-or-launch as set by **-1** through **-9** (default: numbers).

	Valid settings are:

	numbers
	:	Use the number keys 1-9

	functions
	:	Use the function keys F1-F9

	If function keys are used for app launchers the number keys will be used to swap tags, and vice versa.

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

	If a config file is found at **$XDG_CONFIG_HOME/goomwwm/goomwwmrc** or **$HOME/.config/goomwwm/goomwwmrc** or **$HOME/.goomwwmrc**, the first one found will be automatically parsed.

-cycle
:	Set an X11 key name to cycle windows in the same tag and position as the active window (default: XK_c).

	goomwwm -cycle c


-contract
:	Set an X11 key name to contract the active window to fill an underlying space without obscuring any other window in the current tag that would then be fully visible (default: XK_Contract). Opposite of -expand.

	goomwwm -contract BackSpace

-decrease
:	Set an X11 key to decrementally resize the active window (default: shift+page_up).

	goomwwm -decrease shift+page_down

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
:	Set an X11 key name to expand the active window to fill adjacent space without obscuring any other fully visible window in the current tag (default: XK_Return). Opposite of -contract.

	goomwwm -expand Return

-largeleft
:	Set an X11 key to move and resize the active window to cover the left 2/3 of a monitor (default: XK_bracketleft).

	goomwwm -largeleft bracketleft

-largeright
:	Set an X11 key to move and resize the active window to cover the right 2/3 of a monitor (default: XK_bracketright).

	goomwwm -largeleft bracketleft

-launch
:	Set an X11 key to run the application launcher (default: XK_x).

	goomwwm -launch x

-launcher
:	Set a custom application launcher to execute on **Mod-x** (default: dmenu_run).

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
:	Set an X11 key name to switch focus downward from the active window within the current tag (default: XK_k).

	goomwwm -focusdown k

-focusleft
:	Set an X11 key name to switch focus to left of the active window within the current tag (default: XK_j).

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
:	Set an X11 key name to switch focus to right of the active window within the current tag (default: XK_l).

	goomwwm -focusright l

-focusup
:	Set an X11 key name to switch focus upward form the active window within the current tag (default: XK_i).

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
:	Set an X11 key to horizontally tile the active window and others with the same tag, position, and size (default: XK_h).

	goomwwm -htile h

-huntile
:	Set an X11 key to do the opposite of -htile.

	goomwwm -huntile h

-info
:	Set an X11 key to briefly display the active window's title (default: XK_w).

	goomwwm -info w

-increase
:	Set an X11 key to incrementally resize the active window (default: shift+page_up).

	goomwwm -increase shift+page_up

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

-menubc
:	Set the border color (X11 named color or hex #rrggbb) for the window-switcher menu (default: #c0c0c0).

	goomwwm -menubc "#c0c0c0"

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

-minimize
:	Set an X11 key name to minimize a window (default: XK_slash).

	goomwwm -minimize slash

-modkey
:	Change the modifier key mask to any combination of: control,mod1,mod2,mod3,mod4,mod5 (default: mod4).

	goomwwm -modkey control,mod1

-only
:	Set an X11 key name to show only windows in the current tag, hiding everything else (default: XK_o).

	goomwwm -only o

-onlyauto
:	Make **-only** behavior automatic after current tag switch. Note that while this setting makes tags behave pretty much like virtual desktops, it also reduces flexibility.

	goomwwm -onlyauto

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

-prefix
:	Set an X11 key name to act as a modal key combination that replaces the default modifier key for all other combinations (default: none). This is similar to the way key combinations work in **ratpoison** and GNU **screen**.

	goomwwm -prefix z

	Above, **Mod-z** would now need to preceed all other keys. For example, cycling windows would become preass and relases **Mod-z** then press **c**.

	Of course, **-prefix** can also be combined with **-modkey**:

	goomwwm -modkey control -prefix z

	Cycling windows would then become **Control-z** then **c**.

	Finally, if you press the prefix key combination by mistake, press the prefix key again to cancel.

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
	:	Window is raised on Mod-AnyButton click (default for -focusmode sloppy[tag]).

-resizehints
:	How to handle windows that specify resize-increment hints (Default: smart). These are what can sometimes cause tiled terminals to have gaps around the edges.

	goomwwm -resizehints smart

	Valid settings are:

	all
	:	All window hints are respected.

	none
	:	No window hints are respected. Note that this does not prevent windows from sending a follow-up request to be resized to respect their hints. gnome-terminal and lxterminal both do this and may always show gaps.

	smart
	:	Most window hints are respected, except for a few apps we know can handle having their hints ignored. At present, this is **xterm** and **urxvt**.

	(posix regex)
	:	Implies smart mode. A regular expression to match the WM_CLASS of windows to ignore **smart** mode. By default this is "^(xterm|urxvt)$". Regex is case-insensitive using POSIX extended syntax.

-right
:	Set an X11 key name to move the active window to the right in a 3x3 grid (default: XK_Right).

	goomwwm -right Right

-rule
:	Define a global window control rule (default: none). This argument can be specified multiple times to define multiple rules. If a window matches multiple rules only the *last* rule specified is used.

		goomwwm -rule "firefox tag9"
		goomwwm -rule "xfce4-notifyd ignore"
		goomwwm -rule "xterm left,maximize_vert,medium"

	Rules always have the format:

		pattern flag[...,flagN]

	The **pattern** is a case-insensitive POSIX regular expression matched against a window's WM_CLASS, application name, or title fields (in that order). Alternatively, the pattern can be limited to one field by using **class:**, **name:**, or **title:** pattern prefixes (this is also faster):

		goomwwm -rule "class:firefox tag9"
		goomwwm -rule "name:xfce4-notifyd ignore"
		goomwwm -rule "title:xterm left,maximize_vert,medium"

	Valid **flags** are:

	ignore
	:	Do not manage a window. Effectively makes a window behave as it the override_redirect flag is set.

	steal block
	:	Allow or prevent a new widow taking focus.

	reset
	:	Remove all EWMH states and H/V locks (useful for -ruleset).

	once
	:	Allow a rule to execute only once (useful for -ruleset).

	minimize restore
	:	Start window pre-minimzed, or restore a window on rule set switch.

	minimize_auto
	:	Automatically minimize a window when it loses focus.

	tag1 tag2 tag3 tag4 tag5 tag6 tag7 tag8 tag9
	:	Apply tags to a window when it first opens. If the current tag is not in the list the window will not be raised or allowed to take focus.

	monitor1 monitor2 monitor3
	:	Place the window on a specific monitor. These are numbered based on what Xinerama thinks the monitor order should be (ie, usually the same screen numbers as defined in xorg.conf).

	above below fullscreen maximize_horz maximize_vert sticky skip_taskbar skip_pager
	:	Apply respective _NET_WM_STATE_* to a window.

	raise lower
	:	Pre-raise or lower a window in the stacking order. These only take effect for a blocked window. For unblocked windows that take focus, -raisemode takes precedence.

	left right top bottom
	:	Align a window with a screen edge. May be combined. Top trumps bottom. Left trumps right.

	center pointer
	:	Place a window center-screen or centered under the mouse pointer.

	small medium large cover expand contract
	:	Set a window's initial size (same increments as PageUp/Down). May be combined.

	hlock vlock
	:	Lock window horizontally or vertically.

	htile huntile vtile vuntile
	:	Tile or untile a window with its fellows.

	snap_left snap_right snap_up snap_down
	:	Immediately snap a window to another's edge.

	replace
	:	Place a window in the same position as the active window.

	duplicate
	:	Size a window to match the one beneath it.

	NxN N%xN%
	:	Apply a specific size in pixels or percent of monitor size.


	Rules are not currently applied to transient windows (dialogs).

-ruleset
:	Define a group of rules to execute on all windows in the current tag when selected by menu (default: none). See **-runruleset**.

	goomwwm -ruleset Name -rule ... -rule ... -ruleset Name2 -rule ...

	Or, in a config file:

		ruleset Development Layout
		rule class:xterm right,bottom,small
		rule class:gvim left,maximize_vert,large

		ruleset Email/Chat Distractions
		rule class:pidgin left,bottom,small,snap_right
		rule class:chromium top,maximize_horz,large


	All **-ruleset** definitions need to come after the global **-rule** definitions on the command line, or in a config file.

	Where global rules are autonomous and their order is not important, rulesets are more like mini scripts where rules are commands executed in order. Windows may therefore be affected by multiple rules in a ruleset. Use precise regex patterns to be safe.

-runrule
:	Set an X11 key name to reapply any rule relevant to the active window (default: XK_comma).

	goomwwm -runrule comma

-runruleset
:	Set an X11 key name to execute defined rule sets using a menu (default: XK_period).

	goomwwm -runrule period

-shrink
:	Set an X11 key name to decrease the active window size (default: XK_Page_Down) through four basic sizes that tile well together: 1/9th, 1/4th, 4/9th, or fullscreen.

	goomwwm -shrink Page_Down

-snapdown
:	Set an X11 key name to snap the active window downward to the nearest border.

	goomwwm -snapdown Shift+Down

-snapleft
:	Set an X11 key name to snap the active window left to the nearest border.

	goomwwm -snapleft Shift+Left

-snapright
:	Set an X11 key name to snap the active window right to the nearest border.

	goomwwm -snapright Shift+Right

-snapup
:	Set an X11 key name to snap the active window upward to the nearest border.

	goomwwm -snapup Shift+Up

-swapdown
:	Set an X11 key name to swap the active window with one below.

	goomwwm -swapdown Shift+Down

-swapleft
:	Set an X11 key name to swap the active window with one to the left.

	goomwwm -swapleft Shift+Left

-swapright
:	Set an X11 key name to swap the active window with one to the right.

	goomwwm -swapright Shift+Right

-swapup
:	Set an X11 key name to swap the active window with one above.

	goomwwm -swapup Shift+Up

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

-titlebc
:	Set the border color (X11 named color or hex #rrggbb) for window titles (default: #c0c0c0).

	goomwwm -titlebc "#c0c0c0"

-titlebg
:	Set the background text color (X11 named color or hex #rrggbb) for window titles (default: #f2f1f0).

	goomwwm -titlebg "#f2f1f0"

-titlefg
:	Set the foreground text color (X11 named color or hex #rrggbb) for window titles (default: #222222).

	goomwwm -titlefg "#222222"

-titlefont
:	Xft font name for use by window popup titles (default: sans-14).

	goomwwm -titlefont sans-14:medium

-titlebar
:	Height in pixels of a window title bar or on/off (default: off).

	goomwwm -titlebar 18
	goomwwm -titlebar on
	goomwwm -titlebar off

	The **on** argument will auto-calculate titlebar height based on **-titlebarfont**.

-titlebarfont
:	Xft font name for use by window title bars (default: sans-10).

	goomwwm -titlebarfont sans-10:medium

-titlebarfocus
:	Set the text color of focused-window title bars (X11 named color or hex #rrggbb).

	goomwwm -titlebarfocus "#eeeeee"

-titlebarblur
:	Set the text color of unfocused-window title bars (X11 named color or hex #rrggbb).

	goomwwm -titlebarblur "#eeeeee"

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
:	Set an X11 key to vertically tile the active window and other windows with the same tag, position, and size (default: XK_v).

	goomwwm -vtile h

-vuntile
:	Set an X11 key to do the opposite of vtile.

	goomwwm -vuntile h

-warpmode
:	Control whether the mouse pointer warps to a focused window (default: never). This setting can make focusmode **sloppy** more cooperative when focus is changed by means other than the mouse.

	goomwwm -warpmode focus

	Valid settings are:

	never
	:	Pointer is never moved (default for -focusmode click).

	follow
	:	Pointer follows a moved window, but is not moved to a newly focused window.

	focus
	:	Pointer is warped to a newly focused window, and follows a moved window (default for -focusmode sloppy[tag]).


# OPTIONS (cli mode)

When run with **-cli** (command line interface) goomwwm may be used to dispatch commands to another running instance of goomwwm. Valid arguments are:

-duration
:	A time delay in seconds. Currently used only for **-notice**.

	goomwwm -cli -notice "Hello World" -duration 5

-exec
:	Switch to another window manager in place (without restarting X).

	goomwwm -cli -exec dwm

-findstart
:	Locate a window by class, name, or title. If not found, execute it.

	goomwwm -cli -findstart class:xterm

-notice
:	Instruct goomwwm to display something via the popup message box.

	goomwwm -cli -notice "Hello World"

-quit
:	Exit goomwwm.

	goomwwm -cli -quit

-restart
:	Restart a running goomwwm instance in place (without restarting X). Useful for reloading config file or upgrading to a new version.

	goomwwm -cli -restart

-rule
:	Execute a temporary rule on windows in the current tag.

	goomwwm -cli -rule "xterm large"

-ruleset
:	Execute a rule set by name.

	goomwwm -cli -ruleset alpha

# SEE ALSO

**dmenu** (1)

# AUTHOR

Sean Pringle <sean.pringle@gmail.com>
