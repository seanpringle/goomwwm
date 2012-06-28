goomwwm
=======

Get out of my way, Window Manager!

goomwwm is an X11 window manager implemented in C as a cleanroom software project. It manages windows in a minimal floating layout, while providing flexible keyboard-driven controls for window switching, sizing, moving, tagging, and tiling. It is also fast, lightweight, modeless, Xinerama-aware, and EWMH compatible wherever possible.

Keyboard window movement occurs on a 3x3 fullscreen grid. For example, a top-left aligned window moved to the right would reappear top-center, and if moved again, top-right.

Keyboard window sizing moves windows through four basic sizes that tile well: 1/9th of screen (1/3 width, 1/3 height), 1/4th, 4/9th, and fullscreen. Combined with EWMH horizontal and vertical maxmimization, plus some snap-to-edge and expand-to-fill-space controls, manual tiling is easy.

Windows are grouped by tags. Activating a tag raises all windows in the tag. A window may have multiple tags. EWMH panels, pagers, and taskbars see tags as desktops.

_For more detail, see the included man page._

![Alt text](http://aerosuidae.net/goomwwm/goomwwm-1.jpg)

![Alt text](http://aerosuidae.net/goomwwm/goomwwm-2.jpg)

![Alt text](http://aerosuidae.net/goomwwm/goomwwm-3.jpg)

![Alt text](http://aerosuidae.net/goomwwm/goomwwm-4.jpg)