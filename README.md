README
======

This repository contains a patched and further modified version of the
DWM window manager I use, most notably it is linked against libmpdclient
to provide MPD playback and volume control as well as the capability to
bookmark songs and playing times.

Documentation on the MPD patch can be found further down in this file.

The code is based on suckless.org's DWM 6.0 and some patches (see below).

And here is what it looks like (of course, the cool stuff is under the
hood):

![Screenshot](/screenshot.png)

PATCHES & MODIFICATIONS
=======================

'OFFICIAL' PATCHES
------------------
    
    * pango
    * pertag
    * savefloats
    * pushstack
    * cycletag
    * bstack

PERSONAL MODS
-------------

    * You can define filter rules to automatically set the opacity of
      clients (see the section further down)

    * Use XkbKeycodeToKeysym() instead of XKeycodeToKeysym()

    * mpdcmd() callback to directly control mpd. Linked against
      libmpdclient. Provides the Following actions:
        * MpdRaiseVolume
        * MpdLowerVolume
        * MpdMuteVolume
        * MpdToggle (play/pause)
        * MpdPrev
        * MpdNext
        * MpdToggleRepeat
        * MpdToggleConsume
        * MpdToggleRandom
        * MpdToggleSingle

    * mpdcmd_loadpos() and mpdcmd_savepos() callbacks to create and
      access bookmarks. The state is lost when DWM exits. There are
      10 registers 0-9 available for storing up to 10 bookmarks.

    * DCMD() macro for convenient dmenu invocation

    * Compile with -O2 instead of -Os

    * wmii-like actions

DOCUMENTATION: MPD CLIENT EXTENSIONS
====================================

The MPD client extension provides the following bindable callbacks:

```C
    void mpdcmd(const Arg *arg);
    void mpdcmd_playpos(const Arg *arg);
    void mpdcmd_savepos(const Arg *arg);
```

mpdcmd(const Arg \*arg)
----------------------

Control MPD player state and volume.

arg needs to have its member i set to one of the following integer
constants, which are defined as an enum type in dwm.c:

    MpdRaiseVolume
        Raise the MPD mixer volume by voldelta points. voldelta is set
        in config.h.

    MpdLowerVolume
        Opposite of MpdRaiseVolume.

    MpdMuteVolume
        Toggle mute. If unmuted, the volume will be restored to the
        previous level.

    MpdTogglePause
        Toggle play/pause.

    MpdPrev
        Play the previous song in the queue.

    MpdNext
        Play the next song in the queue.

    MpdToggleRepeat
        Toggle the repeat flag.

    MpdToggleConsume
        Toggle the consume flag.

    MpdToggleRandom
        Toggle random playback.

    MpdToggleSingle
        Toggle single player mode.

    MpdUpdate
        Execute a full database update.

mpdcmd_loadpos(const Arg \*arg) / mpdcmd_savepos(const Arg \*arg)
----------------------------------------------------------------

Provides 10 registers to bookmark/restore song and playing position.

args needs to have its member i set to an integer 0 through 9. A call to
mpdcmd_savepos({.i = n}) saves the playlist index and play position of
the currently playing song (includes being paused) into register n. A
corresponding call to mpdcmd_loadpos({.i = n}) plays the song with the
previously saved playlist index from the saved playing position.

The current playlist will also be saved and restored in order to ensure
that the bookmark is being played correctly even if the underlying
playlist has changed in the meantime.

Setting the window opacity / transparency
=========================================

The client filter rules have been extended and now allow setting the
opacity of clients matching a certain rule. The filter table now 
looks like this:

```C
static const const Rule rules[] = {
    /* class                    instance    title       tags mask     isfloating   monitor  opacity */
    { "Iceweasel",              NULL,       NULL,       1 << 1,       False,       -1,      1.0 },
    { "mplayer2",               NULL,       NULL,       1,            True,        -1,      1.0 },
    { "mpv",                    NULL,       NULL,       1,            True,        -1,      1.0 },
    { "URxvt",                  NULL,       NULL,       1 << 0,       False,       -1,      0.9 }};
```

Note the new "opacity" field. The number here must be a double d 0.0 <=
d <= 1.0 with 0.0 meaning 100% transparent (=invisible) and 1.0 opaque
(=no transparency). Values which are out of bounds will be sanitized and
default to 1.0, though.

The default opacity for clients not matching any rule is = 1.0.

changeopacity(const Arg \*arg)
----------------------------

This callback can be used in keybindings. arg needs to have its float
member set to a positive or negative number; when called, the value will
be added to the client's opacity as explained above. Values f > 0.0 will
increase the opacity until its value reaches 1.0, values f < 0.0 will
decrease the opacity. Out of bound values will default to 1.0 (fully
opaque). Example configuration snippet:

```C

    { MODKEY,                       XK_o,      changeopacity,   {.f = +0.05 }},
    { MODKEY|ShiftMask,             XK_o,      changeopacity,   {.f = -0.05 }},
```

setopacity(const Arg \*arg)
--------------------------

Like changeopacity(), but sets the opacity of the focus client to a fix
value.
