goomwwm
=======

Get out of my way, Window Manager!

goomwwm is an X11 window manager implemented in C as a cleanroom software project with the following aims:

* Be fast, lightweight, modeless, minimally decorated, Xinerama-aware, and EWMH compatiable wherever possible.

* Operate in normal window stacking mode with familiar mouse support for window move, resize, and click-to-focus.

	* Support fast keyboard-driven window move, resize, and focus operations along with easy manual tiling of normal floating windows.

	* Windows can be moved in a 3x3 grid on each Xinerama screen and quickly resized through several predefined sizes (1/9th, 1/4th, 4/9th, full) which fit together.

	* As well as the usual ability to maximize fullscreen, horizontally, and vertically, windows can be expanded to fill only adjacent empty space horizontally, vertically, or both.

* Use the Windows key (Mod4, Super) by default for all key combinations. Many key combinations will be familiar, or very close to familiar, to users of mainstream window managers and operating systems.

* Implement a better window-switching solution. Most solutions, such as the traditional Alt-Tab MRU list, simple window stack cycling, or graphical options like Expose, don't support using muscle memory beyond the top two windows or else have excessive visuals that break the user's train-of-thought. Instead:

	* Use tags to group and raise windows (like dwm and friends) controlled via the function keys.

	* Allow the user to assign important applications to key combinations. Eg, switching to Firefox, or starting it when required, should be as easy as (for example) Mod4+1.

	* Have a fast dmenu-like solution for complex window filtering and switching, but more tightly integrated with the Window Manager and centered on screen.

* Use window tags instead of virtual desktops, but pretend the latter when possible for EWMH taskbars and pagers.

For more detail, see the included man page.