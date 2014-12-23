## README

This repository contains a patched and further modified version of the
DWM window manager I use, most notably it is linked against libmpdclient
to provide MPD playback and volume control as well as the capability to
bookmark songs and playing times.

Documentation on the MPD patch can be found further down in this file.

The code is based on suckless.org's DWM 6.0 and some patches (see below).

And here is what it looks like (of course, the cool stuff is under the
hood):

![Screenshot](https://raw.githubusercontent.com/2ion/dwm/gh-pages/screenshot.png)

## PATCHES & MODIFICATIONS

### 'OFFICIAL' PATCHES
    
* pango
* pertag
* savefloats
* pushstack
* cycletag
* bstack
* deck layout

### PERSONAL MODS

* You can define filter rules to automatically set the opacity of
clients (see the section further down)
* Use XkbKeycodeToKeysym() instead of XKeycodeToKeysym()
* mpdcmd() callback to directly control mpd. Linked against
libmpdclient. Provides the Following actions:
  * MpdRaiseVolume
  * MpdLowerVolume
  * MpdMuteVolume
  * MpdToggle (play/pause)
  * MpdPlayAgain
  * MpdPrev
  * MpdNext
  * MpdToggleRepeat
  * MpdToggleConsume
  * MpdToggleRandom
  * MpdToggleSingle
* mpdcmd\_loadpos() and mpdcmd\_savepos() callbacks to create and
access bookmarks. The state is lost when DWM exits. There are
10 registers 0-9 available for storing up to 10 bookmarks.
* libnotify notifications triggered by mpdcmd() actions!
* DCMD() macro for convenient dmenu invocation
* Compile with -O2 instead of -Os
* wmii-like actions

## Building and installing

Due to the extensions, we need the following extra libraries:

* libxft
* libpango
* libpangoxft
* libmpdclient
* libnotify

The Makefile uses pkg-config to obtain CFLAGS and LDFLAGS. If you don't
have pkg-config insalled, you need to adjust the Make config. Then,
```sh
make
make install PREFIX=$PREFIX
```
builds and installs dwm into $PREFIX.

## Configuration premises

config.h is tailored to my main machine, which is running Debian
Unstable, and make assumptions about CPU frequencies, sudo configuration
etc, so you need to review the file in any case. Windowing rules have
been broken out into rules.h.

## MPD client extensions

The MPD client extension provides the following bindable callbacks:

```C
void mpdcmd(const Arg *arg);
void mpdcmd_playpos(const Arg *arg);
void mpdcmd_savepos(const Arg *arg);
```

### mpdcmd(const Arg \*arg)

Control MPD player state and volume.

arg needs to have its member i set to one of the following integer
constants, which are defined as an enum type in dwm.c:

    MpdPlayAgain
        Re-play the currently playing song from the beginning.

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

### mpdcmd\_loadpos(const Arg \*arg) / mpdcmd\_savepos(const Arg \*arg)

Provides 10 registers to bookmark/restore song and playing position.

args needs to have its member i set to an integer 0 through 9. A call to
mpdcmd\_savepos({.i = n}) saves the playlist index and play position of
the currently playing song (includes being paused) into register n. A
corresponding call to mpdcmd\_loadpos({.i = n}) plays the song with the
previously saved playlist index from the saved playing position.

The current playlist will also be saved and restored in order to ensure
that the bookmark is being played correctly even if the underlying
playlist has changed in the meantime.

### Notifications

Notifications with information on the newly selected song are being
displayed if notifications are enabled in config.h. Notifications are
triggered on mpdcmd()'s MpdNext and MpdPrev actions. Naturally, dwm must
have access to a running notification service via dbus. An example
configuration for the lightweight dunst notification daemon can be found
in /resources.

In config.h, the following switches can be set:

```C
// set to 0 or 1 to disable/enable the libnotify codepath
static const int cfg_mpdcmd_notify_enable  = 1;

// times the libnotify code should try to re-initialize if
// a connection attempt to the notification service failed
static const int cfg_mpdcmd_notify_retries = 2;

// seconds how long a single notification should be kept visible
static const int cfg_mpdcmd_notify_timeout = 5;
```

Modifying the notification contents takes a little more effort because
I decided to avoid allowing user templates in order to keep things
simple. You have to re-implement the functions

```C
static void mpdcmd_prevnext_notify(int mpdaction);
static void mpdcmd_notify_settext(MpdcmdNotification *n,
    const char *album, int pos, int queuelen, int minutes, int seconds);
static void mpdcmd_notify_settitle(MpdcmdNotification *n,
    const char *artist, const char *title);
```

where MpdcmdNotification is a defined as

```C
typedef struct {
  char *title;
  char *txt;
} MpdcmdNotification;
```

The function

```C
static void mpdcmd_prevnext_notify(int mpdaction);
```

retrieves the metadata via libmpdclient calls and allocates a
MpdcmdNotification struct. It can decide to take different actions
depending on the mpdaction it was triggered by (MpdPrev|MpdNext). It
then passes the data to

```C
static void mpdcmd_notify_settext(MpdcmdNotification *n, <metadata>);
static void mpdcmd_notify_settitle(MpdcmdNotification *n, <metadata>);
```

which set up the title and txt fields of the MpdcmdNotification as
NULL-terminated strings. They currently use snprintf() and a simple
printf-style format spec to format notification title (field: title) and
body (field: txt).

Finally, mpdcmd\_prevnext\_notify() calls mpdcmd\_notify() with the
notification data as the argument. This function takes care of
everything needed to show the notification.

Here is a screenshot of a notification produced by the default
configuration (using the dunst notification daemon):

![Notification example](https://raw.githubusercontent.com/2ion/dwm/gh-pages/screenshot-notification.png)

## Setting the window opacity / transparency

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

### changeopacity(const Arg \*arg)

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

### setopacity(const Arg \*arg)

Like changeopacity(), but sets the opacity of the focus client to a fix
value.
