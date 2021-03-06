/*
 * DWM with a built-in MPD client and more
 * Copyright 2012-2015 Jens Oliver John
 * See the LICENSE file for license details and attribution.
 */
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <linux/socket.h>

#include <assert.h>
#include <errno.h>
#include <error.h>
#include <fcntl.h>
#include <libnotify/notify.h>
#include <locale.h>
#include <mpd/client.h>
#include <pango/pango.h>
#include <pango/pangoxft.h>
#include <pango/pango-font.h>
#include <pthread.h>
#include <stdarg.h>
#include <signal.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <unistd.h>
#include <math.h>
#include <X11/cursorfont.h>
#include <X11/keysym.h>
#include <X11/XF86keysym.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xproto.h>
#include <X11/Xutil.h>
#include <X11/Xft/Xft.h>
#include <X11/XKBlib.h>
#ifdef XINERAMA
#include <X11/extensions/Xinerama.h>
#endif

#ifndef DEBUG
#define DEBUG 0
#else
#define DEBUG 1
#endif

/* macros */
#define BUTTONMASK                  (ButtonPressMask|ButtonReleaseMask)
#define CLEANMASK(mask)             (mask & ~(numlockmask|LockMask) & (ShiftMask|ControlMask|Mod1Mask|Mod2Mask|Mod3Mask|Mod4Mask|Mod5Mask))
#define INTERSECT(x,y,w,h,m)        (MAX(0, MIN((x)+(w),(m)->wx+(m)->ww) - MAX((x),(m)->wx)) \
                                    * MAX(0, MIN((y)+(h),(m)->wy+(m)->wh) - MAX((y),(m)->wy)))
#define ISVISIBLE(C)                ((C->tags & C->mon->tagset[C->mon->seltags]))
#define LENGTH(X)                   (sizeof X / sizeof X[0])
#ifndef MAX
#define MAX(A, B)                   ((A) > (B) ? (A) : (B))
#endif
#ifndef MIN
#define MIN(A, B)                   ((A) < (B) ? (A) : (B))
#endif
#define MOUSEMASK                   (BUTTONMASK|PointerMotionMask)
#define WIDTH(X)                    ((X)->w + 2 * (X)->bw)
#define HEIGHT(X)                   ((X)->h + 2 * (X)->bw)
#define TAGMASK                     ((1 << LENGTH(tags)) - 1)
#define TEXTW(X)                    (textnw(X, strlen(X)) + dc.font.height)
#define LERROR(status, errnum, ...) if((DEBUG)==1){\
                                      error_at_line((status), (errnum), \
                                        (__func__), (__LINE__), __VA_ARGS__);\
                                    }
#define MPDCMD_BE_CONNECTED         if(mpdcmd_connect() != 0) { \
                                      LERROR(0,0, "mpd_connect() failed"); \
                                      return; \
                                    }
#define OPAQUE                      0xffffffff
#define OPACITY                     "_NET_WM_WINDOW_OPACITY"

/* enums */

enum { CurNormal, CurResize, CurMove, CurLast };                                /* cursor */
enum { ColBorder, ColFG, ColBG, ColLast };                                      /* color */
enum { NetSupported, NetWMName, NetWMState,
       NetWMFullscreen, NetActiveWindow, NetWMWindowType,
       NetWMWindowTypeDialog, NetLast };                                        /* EWMH atoms */
enum { WMProtocols, WMDelete, WMState, WMTakeFocus, WMLast };                   /* default atoms */
enum { ClkTagBar, ClkLtSymbol, ClkWinTitle,
       ClkClientWin, ClkRootWin, ClkLast };                                     /* clicks */

 /* mpdcmd */

enum { MpdLowerVolume,
       MpdMuteVolume,
       MpdNext,
       MpdNotifySong,
       MpdNotifyStatus,
       MpdNotifyVolume,
       MpdPlayAgain,
       MpdPrev,
       MpdToggleConsume,
       MpdTogglePause,
       MpdToggleRandom,
       MpdToggleRepeat,
       MpdToggleSingle,
       MpdToggleWatcher,
       MpdUpdate,
       MpdRaiseVolume };
enum { MpdFlag_Config_ForceOff  = 1<<1,
       MpdFlag_Config_ForceOn   = 1<<2,
       MpdFlag_Config_Respect   = 1<<3 };

/* mpvcmd */

enum
{
  MpvToggle,
  MpvNext,
  MpvPrev,
  MpvMuteVolume,
  MpvRaiseVolume,
  MpvLowerVolume,
  MpvSeekAhead10,
  MpvSeekBehind10,
  MpvSeekAhead30,
  MpvSeekBehind30,
  MpvQuit,
  MpvNextChapter,
  MpvPrevChapter,
  MpvSwitchAudioChannel
};

/* typedefs */

typedef union {
  int i;
  unsigned int ui;
  float f;
  const void *v;
} Arg;

typedef struct {
  unsigned int click;
  unsigned int mask;
  unsigned int button;
  void (*func)(const Arg *arg);
  const Arg arg;
} Button;

typedef struct Monitor Monitor;
typedef struct Client Client;
struct Client {
  char name[256];
  float mina, maxa;
  float cfact;
  int x, y, w, h;
  int sfx, sfy, sfw, sfh; /* stored float geometry, used on mode revert */
  int oldx, oldy, oldw, oldh;
  int basew, baseh, incw, inch, maxw, maxh, minw, minh;
  int bw, oldbw;
  unsigned int tags;
  Bool isfixed, isfloating, isurgent, neverfocus, oldstate, isfullscreen;
  Client *next;
  Client *snext;
  Monitor *mon;
  Window win;
  unsigned int opacity;
};

typedef struct {
  int x, y, w, h;
  unsigned long norm[ColLast];
  unsigned long sel[ColLast];
  Drawable drawable;
  GC gc;

  XftColor  xftnorm[ColLast];
  XftColor  xftsel[ColLast];
  XftDraw  *xftdrawable;

  PangoContext *pgc;
  PangoLayout  *plo;
  PangoFontDescription *pfd;

  struct {
    int ascent;
    int descent;
    int height;
  } font;
} DC; /* draw context */

typedef struct {
  unsigned int mod;
  KeySym keysym;
  void (*func)(const Arg *);
  const Arg arg;
} Key;

typedef struct {
  const char *symbol;
  void (*arrange)(Monitor *);
} Layout;

typedef struct Pertag Pertag;
struct Monitor {
  char ltsymbol[16];
  float mfact;
  int nmaster;
  int num;
  int by;               /* bar geometry */
  int mx, my, mw, mh;   /* screen size */
  int wx, wy, ww, wh;   /* window area  */
  unsigned int seltags;
  unsigned int sellt;
  unsigned int tagset[2];
  Bool showbar;
  Bool topbar;
  Client *clients;
  Client *sel;
  Client *stack;
  Monitor *next;
  Window barwin;
  const Layout *lt[2];
  Pertag *pertag;
};

typedef struct {
  const char *class;
  const char *instance;
  const char *title;
  unsigned int tags;
  Bool isfloating;
  int monitor;
  double opacity;
} Rule;

typedef struct mpd_connection MpdConnection;

typedef struct {
  char *title;
  char *txt;
} MpdcmdNotification;

typedef struct {
  int total;
  int mins;
  int secs;
} MpdcmdSongLength;

typedef struct {
  const char *artist;
  const char *title;
  const char *album;
  int queue_len;
  int queue_pos;
  MpdcmdSongLength len;
} MpdcmdSongInfo;

typedef struct {
  int fd;
  void *mem;
  struct timespec lm;
} Shm;

Shm shm;

/* function declarations */

static Bool applysizehints(Client *c, int *x, int *y, int *w, int *h, Bool interact);
static Bool getrootptr(int *x, int *y);
static Bool gettextprop(Window w, Atom atom, char *text, unsigned int size);
static Bool sendevent(Client *c, Atom proto);
static Bool updategeom(void);
static Client *nexttiled(Client *c);
static Client *wintoclient(Window w);
static Client* prevtiled(Client *c);
static Monitor *createmon(void);
static Monitor *dirtomon(int dir);
static Monitor *recttomon(int x, int y, int w, int h);
static Monitor *wintomon(Window w);
static int mpdcmd_connect(void);
static int mpdcmd_eval_forceflag(int, int);
static int shifttag(int dist);
static int textnw(const char *text, unsigned int len);
static int xerror(Display *dpy, XErrorEvent *ee);
static int xerrordummy(Display *dpy, XErrorEvent *ee);
static int xerrorstart(Display *dpy, XErrorEvent *ee);
static long getstate(Window w);
static unsigned long getcolor(const char *colstr, XftColor *color);
static void applyrules(Client *c);
static void arrange(Monitor *m);
static void arrangemon(Monitor *m);
static void attach(Client *c);
static void attachstack(Client *c);
static void bstack(Monitor *m);
static void buttonpress(XEvent *e);
static void changeopacity(const Arg *arg);
static void checkotherwm(void);
static void cleanup(void);
static void cleanupmon(Monitor *mon);
static void clearurgent(Client *c);
static void clientmessage(XEvent *e);
static void configure(Client *c);
static void configurenotify(XEvent *e);
static void configurerequest(XEvent *e);
static void cycle(const Arg *arg);
static void deck(Monitor *);
static void destroynotify(XEvent *e);
static void detach(Client *c);
static void detachstack(Client *c);
static void die(const char *errstr, ...);
static void drawbar(Monitor *m);
static void drawbars(void);
static void drawtext(const char *text, unsigned long col[ColLast], Bool invert);
static void enternotify(XEvent *e);
static void expose(XEvent *e);
static void focus(Client *c);
static void focusin(XEvent *e);
static void focusmon(const Arg *arg);
static void focusstack(const Arg *arg);
static void grabbuttons(Client *c, Bool focused);
static void grabkeys(void);
static void incnmaster(const Arg *arg);
static void initfont(const char *fontstr);
static void keypress(XEvent *e);
static void killclient(const Arg *arg);
static void manage(Window w, XWindowAttributes *wa);
static void mappingnotify(XEvent *e);
static void maprequest(XEvent *e);
static void monocle(Monitor *m);
static void tcl(Monitor *m);
static void motionnotify(XEvent *e);
static void movemouse(const Arg *arg);
static void mpdcmd(const Arg *arg);
static void mpdcmd_cleanup(void);
static void mpdcmd_free_notification(MpdcmdNotification *n);
static void mpdcmd_init(void);
static void mpdcmd_init_notify(void);
static void mpdcmd_init_registers(void);
static void mpdcmd_loadpos(const Arg *arg);
static void mpdcmd_notify(const MpdcmdNotification*);
static void mpdcmd_notify_settext(MpdcmdNotification *n, const char *album, int pos, int queuelen, int minutes, int seconds);
static void mpdcmd_notify_settitle(MpdcmdNotification *n, const char *artist, const char *title);
static void mpdcmd_notify_statusflags(void);
static void mpdcmd_notify_volume(void);
static void mpdcmd_prevnext(int which, int override_notify);
static void mpdcmd_prevnext_notify(int which);
static void mpdcmd_savepos(const Arg *arg);
static void mpdcmd_toggle_pause(void);
static void mpdcmd_volume(const Arg *arg);
static void *mpdcmd_watcher(void *arg);
static void mpdcmd_start_watcher(void);
static void mpdcmd_stop_watcher(void);
static void mpvcmd(const Arg *a);
static void pop(Client *);
static void propertynotify(XEvent *e);
static void pushdown(const Arg *arg);
static void pushup(const Arg *arg);
static void quit(const Arg *arg);
static void resize(Client *c, int x, int y, int w, int h, Bool interact);
static void resizeclient(Client *c, int x, int y, int w, int h);
static void resizemouse(const Arg *arg);
static void restack(Monitor *m);
static void run(void);
static void scan(void);
static void sendmon(Client *c, Monitor *m);
static void setclientstate(Client *c, long state);
static void setfocus(Client *c);
static void setfullscreen(Client *c, Bool fullscreen);
static void setlayout(const Arg *arg);
static void setmfact(const Arg *arg);
static void setopacity(const Arg *arg);
static void setup(void);
static void showhide(Client *c);
static void sigchld(int unused);
static void sigterm(int unused);
static void spawn(const Arg *arg);
static void tag(const Arg *arg);
static void tagcycle(const Arg *arg);
static void tagmon(const Arg *arg);
static void tile(Monitor *);
static void togglebar(const Arg *arg);
static void togglefloating(const Arg *arg);
static void toggletag(const Arg *arg);
static void toggleview(const Arg *arg);
static void unfocus(Client *c, Bool setfocus);
static void unmanage(Client *c, Bool destroyed);
static void unmapnotify(XEvent *e);
static void updatebarpos(Monitor *m);
static void updatebars(void);
static void updatenumlockmask(void);
static void updateopacity(Client *c);
static void updatesizehints(Client *c);
static void updatetitle(Client *c);
static void updatewindowtype(Client *c);
static void updatewmhints(Client *c);
static void view(const Arg *arg);
static void zoom(const Arg *arg);

/* variables */

static const char broken[] = "broken";
static int screen;
static int sw, sh;           /* X display screen geometry width, height */
static int bh, blw = 0;      /* bar geometry */
static int (*xerrorxlib)(Display *, XErrorEvent *);
static unsigned int numlockmask = 0;
static void (*handler[LASTEvent]) (XEvent *) = {
  [ButtonPress] = buttonpress,
  [ClientMessage] = clientmessage,
  [ConfigureRequest] = configurerequest,
  [ConfigureNotify] = configurenotify,
  [DestroyNotify] = destroynotify,
  [EnterNotify] = enternotify,
  [Expose] = expose,
  [FocusIn] = focusin,
  [KeyPress] = keypress,
  [MappingNotify] = mappingnotify,
  [MapRequest] = maprequest,
  [MotionNotify] = motionnotify,
  [PropertyNotify] = propertynotify,
  [UnmapNotify] = unmapnotify
};
static Atom wmatom[WMLast], netatom[NetLast];
static Bool running = True;
static Cursor cursor[CurLast];
static Display *dpy;
static DC dc;
static Monitor *mons = NULL, *selmon = NULL;
static Window root;
static MpdConnection *mpdc = NULL;
int MpdcmdRegister[10][4];
char MpdCmdRegisterPlaylists[10][256];
int MpdcmdCanNotify = 0;
volatile int mpd_watcher_exists = 0;
pthread_t mpd_watcher_thread;

/* configuration, allows nested code to access above variables */

#include "config.h"

struct Pertag {
  unsigned int curtag, prevtag; /* current and previous tag */
  int nmasters[LENGTH(tags) + 1]; /* number of windows in master area */
  float mfacts[LENGTH(tags) + 1]; /* mfacts per tag */
  unsigned int sellts[LENGTH(tags) + 1]; /* selected layouts */
  const Layout *ltidxs[LENGTH(tags) + 1][2]; /* matrix of tags and layouts indexes  */
  Bool showbars[LENGTH(tags) + 1]; /* display bar for the current tag */
};

/* compile-time check if all tags fit into an unsigned int bit array. */

struct NumTags { char limitexceeded[LENGTH(tags) > 31 ? -1 : 1]; };

/* function implementations */

static void
bstack(Monitor *m) {
  int w, h, mh, mx, tx, ty, tw;
  unsigned int i, n;
  Client *c;

  for(n = 0, c = nexttiled(m->clients); c; c = nexttiled(c->next), n++);
  if(n == 0)
    return;
  if(n > m->nmaster) {
    mh = m->nmaster ? m->mfact * m->wh : 0;
    tw = m->ww / (n - m->nmaster);
    ty = m->wy + mh;
  }
  else {
    mh = m->wh;
    tw = m->ww;
    ty = m->wy;
  }
  for(i = mx = 0, tx = m->wx, c = nexttiled(m->clients); c; c = nexttiled(c->next), i++) {
    if(i < m->nmaster) {
      w = (m->ww - mx) / (MIN(n, m->nmaster) - i);
      resize(c, m->wx + mx, m->wy, w - (2 * c->bw), mh - (2 * c->bw), False);
      mx += WIDTH(c);
    }
    else {
      h = m->wh - mh;
      resize(c, tx, ty, tw - (2 * c->bw), h - (2 * c->bw), False);
      if(tw != m->ww)
        tx += WIDTH(c);
    }
  }
}

static Client *
prevtiled(Client *c) {
  Client *p, *r;

  for(p = selmon->clients, r = NULL; p && p != c; p = p->next)
    if(!p->isfloating && ISVISIBLE(p))
      r = p;
  return r;
}

static void
pushup(const Arg *arg) {
  Client *sel = selmon->sel;
  Client *c;

  if(!sel || sel->isfloating)
    return;
  if((c = prevtiled(sel))) {
    /* attach before c */
    detach(sel);
    sel->next = c;
    if(selmon->clients == c)
      selmon->clients = sel;
    else {
      for(c = selmon->clients; c->next != sel->next; c = c->next);
      c->next = sel;
    }
  } else {
    /* move to the end */
    for(c = sel; c->next; c = c->next);
    detach(sel);
    sel->next = NULL;
    c->next = sel;
  }
  focus(sel);
  arrange(selmon);
}

static void
pushdown(const Arg *arg) {
  Client *sel = selmon->sel;
  Client *c;

  if(!sel || sel->isfloating)
    return;
  if((c = nexttiled(sel->next))) {
    /* attach after c */
    detach(sel);
    sel->next = c->next;
    c->next = sel;
  } else {
    /* move to the front */
    detach(sel);
    attach(sel);
  }
  focus(sel);
  arrange(selmon);
}

void
applyrules(Client *c) {
  const char *class, *instance;
  unsigned int i;
  double d;
  const Rule *r;
  Monitor *m;
  XClassHint ch = { NULL, NULL };

  /* rule matching */
  c->isfloating = c->tags = 0;
  XGetClassHint(dpy, c->win, &ch);
  class    = ch.res_class ? ch.res_class : broken;
  instance = ch.res_name  ? ch.res_name  : broken;

  for(i = 0; i < LENGTH(rules); i++) {
    r = &rules[i];
    if((!r->title || strstr(c->name, r->title))
    && (!r->class || strstr(class, r->class))
    && (!r->instance || strstr(instance, r->instance)))
    {
      c->isfloating = r->isfloating;
      c->tags |= r->tags;
      for(m = mons; m && m->num != r->monitor; m = m->next);
      if(m)
        c->mon = m;
      // opacity; see updateopacity()
      d = r->opacity;
      if(d > 1.0 || d < 0.0)
          d = 1.0;
      c->opacity = (unsigned int) (d * OPAQUE);
    }
  }
  if(ch.res_class)
    XFree(ch.res_class);
  if(ch.res_name)
    XFree(ch.res_name);
  c->tags = c->tags & TAGMASK ? c->tags & TAGMASK : c->mon->tagset[c->mon->seltags];
}

void
updateopacity(Client *c)
{
  if(c->opacity == OPAQUE) {
    switch(XDeleteProperty(dpy, c->win, XInternAtom(dpy, OPACITY, False))) {
      case BadAtom:
      case BadWindow:
        LERROR(0,0, "XDeleteProperty() failed");
      default:
        break;
    }
  }
  else {
    switch(XChangeProperty(dpy, c->win, XInternAtom(dpy, OPACITY, False),
            XA_CARDINAL, 32, PropModeReplace,
            (unsigned char*) &c->opacity, 1L)) {
      case BadAlloc:
      case BadAtom:
      case BadMatch:
      case BadValue:
      case BadWindow:
        LERROR(0,0, "XChangeProperty() failed");
      default:
        break;
    }
  }
//    XSync(dpy, False);
}

void
changeopacity(const Arg *arg)
{
  Client *c;
  double opacity;
  double delta = (double) arg->f;

  if(!(c = selmon->sel))
    return;
  opacity = ((double) c->opacity / OPAQUE) + delta;
  if(opacity > 1.0 || opacity < 0.0) {
    opacity = 1.0;
  }
  c->opacity = (unsigned int) (opacity * (double) OPAQUE);
  updateopacity(c);
}

void
setopacity(const Arg *arg)
{
  Client *c;
  double opacity = (arg->f <= 1.0 && arg->f >= 0.0) ? arg->f : 1.0;

  if(!(c = selmon->sel))
      return;
  c->opacity = (unsigned int) (opacity * (double) OPAQUE);
  updateopacity(c);
}

Bool
applysizehints(Client *c, int *x, int *y, int *w, int *h, Bool interact) {
  Bool baseismin;
  Monitor *m = c->mon;

  /* set minimum possible */
  *w = MAX(1, *w);
  *h = MAX(1, *h);
  if(interact) {
    if(*x > sw)
      *x = sw - WIDTH(c);
    if(*y > sh)
      *y = sh - HEIGHT(c);
    if(*x + *w + 2 * c->bw < 0)
      *x = 0;
    if(*y + *h + 2 * c->bw < 0)
      *y = 0;
  }
  else {
    if(*x >= m->wx + m->ww)
      *x = m->wx + m->ww - WIDTH(c);
    if(*y >= m->wy + m->wh)
      *y = m->wy + m->wh - HEIGHT(c);
    if(*x + *w + 2 * c->bw <= m->wx)
      *x = m->wx;
    if(*y + *h + 2 * c->bw <= m->wy)
      *y = m->wy;
  }
  if(*h < bh)
    *h = bh;
  if(*w < bh)
    *w = bh;
  if(resizehints || c->isfloating || !c->mon->lt[c->mon->sellt]->arrange) {
    /* see last two sentences in ICCCM 4.1.2.3 */
    baseismin = c->basew == c->minw && c->baseh == c->minh;
    if(!baseismin) { /* temporarily remove base dimensions */
      *w -= c->basew;
      *h -= c->baseh;
    }
    /* adjust for aspect limits */
    if(c->mina > 0 && c->maxa > 0) {
      if(c->maxa < (float)*w / *h)
        *w = *h * c->maxa + 0.5;
      else if(c->mina < (float)*h / *w)
        *h = *w * c->mina + 0.5;
    }
    if(baseismin) { /* increment calculation requires this */
      *w -= c->basew;
      *h -= c->baseh;
    }
    /* adjust for increment value */
    if(c->incw)
      *w -= *w % c->incw;
    if(c->inch)
      *h -= *h % c->inch;
    /* restore base dimensions */
    *w = MAX(*w + c->basew, c->minw);
    *h = MAX(*h + c->baseh, c->minh);
    if(c->maxw)
      *w = MIN(*w, c->maxw);
    if(c->maxh)
      *h = MIN(*h, c->maxh);
  }
  return *x != c->x || *y != c->y || *w != c->w || *h != c->h;
}

void
arrange(Monitor *m) {
  if(m)
    showhide(m->stack);
  else for(m = mons; m; m = m->next)
    showhide(m->stack);
  if(m)
    arrangemon(m);
  else for(m = mons; m; m = m->next)
    arrangemon(m);
}

void
arrangemon(Monitor *m) {
  strncpy(m->ltsymbol, m->lt[m->sellt]->symbol, sizeof m->ltsymbol);
  if(m->lt[m->sellt]->arrange)
    m->lt[m->sellt]->arrange(m);
  restack(m);
}

void
attach(Client *c) {
  c->next = c->mon->clients;
  c->mon->clients = c;
}

void
attachstack(Client *c) {
  c->snext = c->mon->stack;
  c->mon->stack = c;
}

void
buttonpress(XEvent *e) {
  unsigned int i, x, click;
  Arg arg = {0};
  Client *c;
  Monitor *m;
  XButtonPressedEvent *ev = &e->xbutton;

  click = ClkRootWin;
  /* focus monitor if necessary */
  if((m = wintomon(ev->window)) && m != selmon) {
    unfocus(selmon->sel, True);
    selmon = m;
    focus(NULL);
  }
  if(ev->window == selmon->barwin) {
    i = x = 0;
    do
      x += TEXTW(tags[i]);
    while(ev->x >= x && ++i < LENGTH(tags));
    if(i < LENGTH(tags)) {
      click = ClkTagBar;
      arg.ui = 1 << i;
    }
    else if(ev->x < x + blw)
      click = ClkLtSymbol;
    else
      click = ClkWinTitle;
  }
  else if((c = wintoclient(ev->window))) {
    focus(c);
    click = ClkClientWin;
  }
  for(i = 0; i < LENGTH(buttons); i++)
    if(click == buttons[i].click && buttons[i].func && buttons[i].button == ev->button
    && CLEANMASK(buttons[i].mask) == CLEANMASK(ev->state))
      buttons[i].func(click == ClkTagBar && buttons[i].arg.i == 0 ? &arg : &buttons[i].arg);
}

void
checkotherwm(void) {
  xerrorxlib = XSetErrorHandler(xerrorstart);
  /* this causes an error if some other window manager is running */
  XSelectInput(dpy, DefaultRootWindow(dpy), SubstructureRedirectMask);
  XSync(dpy, False);
  XSetErrorHandler(xerror);
  XSync(dpy, False);
}

void
cleanup(void) {
  Arg a = {.ui = ~0};
  Layout foo = { "", NULL };
  Monitor *m;

  view(&a);
  selmon->lt[selmon->sellt] = &foo;
  for(m = mons; m; m = m->next)
    while(m->stack)
      unmanage(m->stack, False);
  XUngrabKey(dpy, AnyKey, AnyModifier, root);
  XFreePixmap(dpy, dc.drawable);
  XFreeGC(dpy, dc.gc);
  XFreeCursor(dpy, cursor[CurNormal]);
  XFreeCursor(dpy, cursor[CurResize]);
  XFreeCursor(dpy, cursor[CurMove]);
  while(mons)
    cleanupmon(mons);
  XSync(dpy, False);
  XSetInputFocus(dpy, PointerRoot, RevertToPointerRoot, CurrentTime);
  mpdcmd_cleanup();
}

void
cleanupmon(Monitor *mon) {
  Monitor *m;

  if(mon == mons)
    mons = mons->next;
  else {
    for(m = mons; m && m->next != mon; m = m->next);
    m->next = mon->next;
  }
  XUnmapWindow(dpy, mon->barwin);
  XDestroyWindow(dpy, mon->barwin);
  free(mon);
}

void
clearurgent(Client *c) {
  XWMHints *wmh;

  c->isurgent = False;
  if(!(wmh = XGetWMHints(dpy, c->win)))
    return;
  wmh->flags &= ~XUrgencyHint;
  XSetWMHints(dpy, c->win, wmh);
  XFree(wmh);
}

void
clientmessage(XEvent *e) {
  XClientMessageEvent *cme = &e->xclient;
  Client *c = wintoclient(cme->window);

  if(!c)
    return;
  if(cme->message_type == netatom[NetWMState]) {
    if(cme->data.l[1] == netatom[NetWMFullscreen] || cme->data.l[2] == netatom[NetWMFullscreen])
      setfullscreen(c, (cme->data.l[0] == 1 /* _NET_WM_STATE_ADD    */
                    || (cme->data.l[0] == 2 /* _NET_WM_STATE_TOGGLE */ && !c->isfullscreen)));
  }
  else if(cme->message_type == netatom[NetActiveWindow]) {
    if(!ISVISIBLE(c)) {
      c->mon->seltags ^= 1;
      c->mon->tagset[c->mon->seltags] = c->tags;
    }
    pop(c);
  }
}

void
configure(Client *c) {
  XConfigureEvent ce;

  ce.type = ConfigureNotify;
  ce.display = dpy;
  ce.event = c->win;
  ce.window = c->win;
  ce.x = c->x;
  ce.y = c->y;
  ce.width = c->w;
  ce.height = c->h;
  ce.border_width = c->bw;
  ce.above = None;
  ce.override_redirect = False;
  XSendEvent(dpy, c->win, False, StructureNotifyMask, (XEvent *)&ce);
}

void
configurenotify(XEvent *e) {
  Monitor *m;
  Client *c;
  XConfigureEvent *ev = &e->xconfigure;
  Bool dirty;

  if(ev->window == root) {
    dirty = (sw != ev->width);
    sw = ev->width;
    sh = ev->height;
    if(updategeom() || dirty) {
      if(dc.drawable != 0)
        XFreePixmap(dpy, dc.drawable);
      dc.drawable = XCreatePixmap(dpy, root, sw, bh, DefaultDepth(dpy, screen));
      XftDrawChange(dc.xftdrawable, dc.drawable);
      updatebars();
      for(m = mons; m; m = m->next) {
        for(c = m->clients; c; c = c->next)
          if(c->isfullscreen)
            resizeclient(c, m->mx, m->my, m->mw, m->mh);
        XMoveResizeWindow(dpy, m->barwin, m->wx, m->by, m->ww, bh);
      }
      focus(NULL);
      arrange(NULL);
    }
  }
}

void
configurerequest(XEvent *e) {
  Client *c;
  Monitor *m;
  XConfigureRequestEvent *ev = &e->xconfigurerequest;
  XWindowChanges wc;

  if((c = wintoclient(ev->window))) {
    if(ev->value_mask & CWBorderWidth)
      c->bw = ev->border_width;
    else if(c->isfloating || !selmon->lt[selmon->sellt]->arrange) {
      m = c->mon;
      if(ev->value_mask & CWX) {
        c->oldx = c->x;
        c->x = m->mx + ev->x;
      }
      if(ev->value_mask & CWY) {
        c->oldy = c->y;
        c->y = m->my + ev->y;
      }
      if(ev->value_mask & CWWidth) {
        c->oldw = c->w;
        c->w = ev->width;
      }
      if(ev->value_mask & CWHeight) {
        c->oldh = c->h;
        c->h = ev->height;
      }
      if((c->x + c->w) > m->mx + m->mw && c->isfloating)
        c->x = m->mx + (m->mw / 2 - WIDTH(c) / 2); /* center in x direction */
      if((c->y + c->h) > m->my + m->mh && c->isfloating)
        c->y = m->my + (m->mh / 2 - HEIGHT(c) / 2); /* center in y direction */
      if((ev->value_mask & (CWX|CWY)) && !(ev->value_mask & (CWWidth|CWHeight)))
        configure(c);
      if(ISVISIBLE(c))
        XMoveResizeWindow(dpy, c->win, c->x, c->y, c->w, c->h);
    }
    else
      configure(c);
  }
  else {
    wc.x = ev->x;
    wc.y = ev->y;
    wc.width = ev->width;
    wc.height = ev->height;
    wc.border_width = ev->border_width;
    wc.sibling = ev->above;
    wc.stack_mode = ev->detail;
    XConfigureWindow(dpy, ev->window, ev->value_mask, &wc);
  }
  XSync(dpy, False);
}

Monitor *
createmon(void) {
  Monitor *m;
  int i;

  if(!(m = (Monitor *)calloc(1, sizeof(Monitor))))
    die("fatal: could not malloc() %u bytes\n", sizeof(Monitor));
  m->tagset[0] = m->tagset[1] = 1;
  m->mfact = mfact;
  m->nmaster = nmaster;
  m->showbar = showbar;
  m->topbar = topbar;
  m->lt[0] = &layouts[0];
  m->lt[1] = &layouts[1 % LENGTH(layouts)];
  strncpy(m->ltsymbol, layouts[0].symbol, sizeof m->ltsymbol);
  if(!(m->pertag = (Pertag *)calloc(1, sizeof(Pertag))))
    die("fatal: could not malloc() %u bytes\n", sizeof(Pertag));
  m->pertag->curtag = m->pertag->prevtag = 1;
  for(i=0; i <= LENGTH(tags); i++) {
    /* init nmaster */
    m->pertag->nmasters[i] = m->nmaster;

    /* init mfacts */
    m->pertag->mfacts[i] = m->mfact;

    /* init layouts */
    m->pertag->ltidxs[i][0] = m->lt[0];
    m->pertag->ltidxs[i][1] = m->lt[1];
    m->pertag->sellts[i] = m->sellt;

    /* init showbar */
    m->pertag->showbars[i] = m->showbar;
  }
  return m;
}

void
destroynotify(XEvent *e) {
  Client *c;
  XDestroyWindowEvent *ev = &e->xdestroywindow;

  if((c = wintoclient(ev->window)))
    unmanage(c, True);
}

void
detach(Client *c) {
  Client **tc;

  for(tc = &c->mon->clients; *tc && *tc != c; tc = &(*tc)->next);
  *tc = c->next;
}

void
detachstack(Client *c) {
  Client **tc, *t;

  for(tc = &c->mon->stack; *tc && *tc != c; tc = &(*tc)->snext);
  *tc = c->snext;

  if(c == c->mon->sel) {
    for(t = c->mon->stack; t && !ISVISIBLE(t); t = t->snext);
    c->mon->sel = t;
  }
}

void
die(const char *errstr, ...) {
  va_list ap;

  va_start(ap, errstr);
  vfprintf(stderr, errstr, ap);
  va_end(ap);
  exit(EXIT_FAILURE);
}

Monitor *
dirtomon(int dir) {
  Monitor *m = NULL;

  if(dir > 0) {
    if(!(m = selmon->next))
      m = mons;
  }
  else if(selmon == mons)
    for(m = mons; m->next; m = m->next);
  else
    for(m = mons; m->next != selmon; m = m->next);
  return m;
}

void
drawbar(Monitor *m) {
  int x;
  unsigned int i, occ = 0, urg = 0;
  unsigned long *col;
  Client *c;

  for(c = m->clients; c; c = c->next) {
    occ |= c->tags;
    if(c->isurgent)
      urg |= c->tags;
  }
  dc.x = 0;
  for(i = 0; i < LENGTH(tags); i++) {
    dc.w = TEXTW(tags[i]);
    col = m->tagset[m->seltags] & 1 << i ? dc.sel : dc.norm;
    drawtext(tags[i], col, urg & 1 << i);
    dc.x += dc.w;
  }
  dc.w = blw = TEXTW(m->ltsymbol);
  drawtext(m->ltsymbol, dc.norm, False);
  dc.x += dc.w;
  x = dc.x;
  dc.x = m->ww;
  if((dc.w = dc.x - x) > bh) {
    dc.x = x;
    if(m->sel) {
      col = m == selmon ? dc.sel : dc.norm;
      drawtext(m->sel->name, col, False);
    }
    else
      drawtext(NULL, dc.norm, False);
  }
  XCopyArea(dpy, dc.drawable, m->barwin, dc.gc, 0, 0, m->ww, bh, 0, 0);
  XSync(dpy, False);
}

void
drawbars(void) {
  Monitor *m;

  for(m = mons; m; m = m->next)
    drawbar(m);
}

void
drawtext(const char *text, unsigned long col[ColLast], Bool invert) {
  char buf[256];
  int i, x, y, h, len, olen;

  XSetForeground(dpy, dc.gc, col[invert ? ColFG : ColBG]);
  XFillRectangle(dpy, dc.drawable, dc.gc, dc.x, dc.y, dc.w, dc.h);
  if(!text)
    return;
  olen = strlen(text);
  h = dc.font.ascent + dc.font.descent;
  y = dc.y;
  x = dc.x + (h / 2);
  /* shorten text if necessary */
  for(len = MIN(olen, sizeof buf); len && textnw(text, len) > dc.w - h; len--);
  if(!len)
    return;
  memcpy(buf, text, len);
  if(len < olen)
    for(i = len; i && i > len - 3; buf[--i] = '.');
  pango_layout_set_text(dc.plo, text, len);
  pango_xft_render_layout(dc.xftdrawable, (col==dc.norm?dc.xftnorm:dc.xftsel)+(invert?ColBG:ColFG), dc.plo, x * PANGO_SCALE, y * PANGO_SCALE);
}

void
enternotify(XEvent *e) {
  Client *c;
  Monitor *m;
  XCrossingEvent *ev = &e->xcrossing;

  if((ev->mode != NotifyNormal || ev->detail == NotifyInferior) && ev->window != root)
    return;
  c = wintoclient(ev->window);
  m = c ? c->mon : wintomon(ev->window);
  if(m != selmon) {
    unfocus(selmon->sel, True);
    selmon = m;
  }
  else if(!c || c == selmon->sel)
    return;
  focus(c);
}

void
expose(XEvent *e) {
  Monitor *m;
  XExposeEvent *ev = &e->xexpose;

  if(ev->count == 0 && (m = wintomon(ev->window)))
    drawbar(m);
}

void
focus(Client *c) {
  if(!c || !ISVISIBLE(c))
    for(c = selmon->stack; c && !ISVISIBLE(c); c = c->snext);
  /* was if(selmon->sel) */
  if(selmon->sel && selmon->sel != c)
    unfocus(selmon->sel, False);
  if(c) {
    if(c->mon != selmon)
      selmon = c->mon;
    if(c->isurgent)
      clearurgent(c);
    detachstack(c);
    attachstack(c);
    grabbuttons(c, True);
    XSetWindowBorder(dpy, c->win, dc.sel[ColBorder]);
    setfocus(c);
  }
  else
    XSetInputFocus(dpy, root, RevertToPointerRoot, CurrentTime);
  selmon->sel = c;
  drawbars();
}

void
focusin(XEvent *e) { /* there are some broken focus acquiring clients */
  XFocusChangeEvent *ev = &e->xfocus;

  if(selmon->sel && ev->window != selmon->sel->win)
    setfocus(selmon->sel);
}

void
focusmon(const Arg *arg) {
  Monitor *m;

  if(!mons->next)
    return;
  if((m = dirtomon(arg->i)) == selmon)
    return;
  unfocus(selmon->sel, True);
  selmon = m;
  focus(NULL);
}

void
focusstack(const Arg *arg) {
  Client *c = NULL, *i;

  if(!selmon->sel)
    return;
  if(arg->i > 0) {
    for(c = selmon->sel->next; c && !ISVISIBLE(c); c = c->next);
    if(!c)
      for(c = selmon->clients; c && !ISVISIBLE(c); c = c->next);
  }
  else {
    for(i = selmon->clients; i != selmon->sel; i = i->next)
      if(ISVISIBLE(i))
        c = i;
    if(!c)
      for(; i; i = i->next)
        if(ISVISIBLE(i))
          c = i;
  }
  if(c) {
    focus(c);
    restack(selmon);
  }
}

Atom
getatomprop(Client *c, Atom prop) {
  int di;
  unsigned long dl;
  unsigned char *p = NULL;
  Atom da, atom = None;

  if(XGetWindowProperty(dpy, c->win, prop, 0L, sizeof atom, False, XA_ATOM,
                        &da, &di, &dl, &dl, &p) == Success && p) {
    atom = *(Atom *)p;
    XFree(p);
  }
  return atom;
}

unsigned long
getcolor(const char *colstr, XftColor *color) {
  Colormap cmap = DefaultColormap(dpy, screen);
  Visual *vis = DefaultVisual(dpy, screen);

  if(!XftColorAllocName(dpy,vis,cmap,colstr, color))
    die("error, cannot allocate color '%s'\n", colstr);
  return color->pixel;
}

Bool
getrootptr(int *x, int *y) {
  int di;
  unsigned int dui;
  Window dummy;

  return XQueryPointer(dpy, root, &dummy, &dummy, x, y, &di, &di, &dui);
}

long
getstate(Window w) {
  int format;
  long result = -1;
  unsigned char *p = NULL;
  unsigned long n, extra;
  Atom real;

  if(XGetWindowProperty(dpy, w, wmatom[WMState], 0L, 2L, False, wmatom[WMState],
                        &real, &format, &n, &extra, (unsigned char **)&p) != Success)
    return -1;
  if(n != 0)
    result = *p;
  XFree(p);
  return result;
}

Bool
gettextprop(Window w, Atom atom, char *text, unsigned int size) {
  char **list = NULL;
  int n;
  XTextProperty name;

  if(!text || size == 0)
    return False;
  text[0] = '\0';
  XGetTextProperty(dpy, w, &name, atom);
  if(!name.nitems)
    return False;
  if(name.encoding == XA_STRING)
    strncpy(text, (char *)name.value, size - 1);
  else {
    if(XmbTextPropertyToTextList(dpy, &name, &list, &n) >= Success && n > 0 && *list) {
      strncpy(text, *list, size - 1);
      XFreeStringList(list);
    }
  }
  text[size - 1] = '\0';
  XFree(name.value);
  return True;
}

void
grabbuttons(Client *c, Bool focused) {
  updatenumlockmask();
  {
    unsigned int i, j;
    unsigned int modifiers[] = { 0, LockMask, numlockmask, numlockmask|LockMask };
    XUngrabButton(dpy, AnyButton, AnyModifier, c->win);
    if(focused) {
      for(i = 0; i < LENGTH(buttons); i++)
        if(buttons[i].click == ClkClientWin)
          for(j = 0; j < LENGTH(modifiers); j++)
            XGrabButton(dpy, buttons[i].button,
                        buttons[i].mask | modifiers[j],
                        c->win, False, BUTTONMASK,
                        GrabModeAsync, GrabModeSync, None, None);
    }
    else
      XGrabButton(dpy, AnyButton, AnyModifier, c->win, False,
                  BUTTONMASK, GrabModeAsync, GrabModeSync, None, None);
  }
}

void
grabkeys(void) {
  updatenumlockmask();
  {
    unsigned int i, j;
    unsigned int modifiers[] = { 0, LockMask, numlockmask, numlockmask|LockMask };
    KeyCode code;

    XUngrabKey(dpy, AnyKey, AnyModifier, root);
    for(i = 0; i < LENGTH(keys); i++)
      if((code = XKeysymToKeycode(dpy, keys[i].keysym)))
        for(j = 0; j < LENGTH(modifiers); j++)
          XGrabKey(dpy, code, keys[i].mod | modifiers[j], root,
             True, GrabModeAsync, GrabModeAsync);
  }
}

void
incnmaster(const Arg *arg) {
  selmon->nmaster = selmon->pertag->nmasters[selmon->pertag->curtag] = MAX(selmon->nmaster + arg->i, 0);
  arrange(selmon);
}

void
initfont(const char *fontstr) {
  PangoFontMetrics *metrics;

  dc.pgc = pango_xft_get_context(dpy, screen);
  dc.pfd = pango_font_description_from_string(fontstr);

  metrics = pango_context_get_metrics(dc.pgc, dc.pfd, pango_language_from_string(setlocale(LC_CTYPE, "")));
  dc.font.ascent = pango_font_metrics_get_ascent(metrics) / PANGO_SCALE;
  dc.font.descent = pango_font_metrics_get_descent(metrics) / PANGO_SCALE;

  pango_font_metrics_unref(metrics);

  dc.plo = pango_layout_new(dc.pgc);
  pango_layout_set_font_description(dc.plo, dc.pfd);
  dc.font.height = dc.font.ascent + dc.font.descent;
}

#ifdef XINERAMA
static Bool
isuniquegeom(XineramaScreenInfo *unique, size_t n, XineramaScreenInfo *info) {
  while(n--)
    if(unique[n].x_org == info->x_org && unique[n].y_org == info->y_org
    && unique[n].width == info->width && unique[n].height == info->height)
      return False;
  return True;
}
#endif /* XINERAMA */

void
keypress(XEvent *e) {
  unsigned int i;
  KeySym keysym;
  XKeyEvent *ev;

  ev = &e->xkey;
  keysym = XkbKeycodeToKeysym(dpy, (KeyCode)ev->keycode, 0, 0);
  for(i = 0; i < LENGTH(keys); i++)
    if(keysym == keys[i].keysym
    && CLEANMASK(keys[i].mod) == CLEANMASK(ev->state)
    && keys[i].func)
      keys[i].func(&(keys[i].arg));
}

void
killclient(const Arg *arg) {
  if(!selmon->sel)
    return;
  if(!sendevent(selmon->sel, wmatom[WMDelete])) {
    XGrabServer(dpy);
    XSetErrorHandler(xerrordummy);
    XSetCloseDownMode(dpy, DestroyAll);
    XKillClient(dpy, selmon->sel->win);
    XSync(dpy, False);
    XSetErrorHandler(xerror);
    XUngrabServer(dpy);
  }
}

void
manage(Window w, XWindowAttributes *wa) {
  Client *c, *t = NULL;
  Window trans = None;
  XWindowChanges wc;

  if(!(c = calloc(1, sizeof(Client))))
    die("fatal: could not malloc() %u bytes\n", sizeof(Client));
  c->win = w;
  c->opacity = OPAQUE;

  /* geometry */
  c->x = c->oldx = wa->x;
  c->y = c->oldy = wa->y;
  c->w = c->oldw = wa->width;
  c->h = c->oldh = wa->height;
  c->oldbw = wa->border_width;
  c->cfact = 1.0;

  updatetitle(c);
  if(XGetTransientForHint(dpy, w, &trans) && (t = wintoclient(trans))) {
    c->mon = t->mon;
    c->tags = t->tags;
  }
  else {
    c->mon = selmon;
    applyrules(c);
  }

  if(c->x + WIDTH(c) > c->mon->mx + c->mon->mw)
    c->x = c->mon->mx + c->mon->mw - WIDTH(c);
  if(c->y + HEIGHT(c) > c->mon->my + c->mon->mh)
    c->y = c->mon->my + c->mon->mh - HEIGHT(c);
  c->x = MAX(c->x, c->mon->mx);
  /* only fix client y-offset, if the client center might cover the bar */
  c->y = MAX(c->y, ((c->mon->by == c->mon->my) && (c->x + (c->w / 2) >= c->mon->wx)
             && (c->x + (c->w / 2) < c->mon->wx + c->mon->ww)) ? bh : c->mon->my);
  c->bw = borderpx;

  wc.border_width = c->bw;
  XConfigureWindow(dpy, w, CWBorderWidth, &wc);
  XSetWindowBorder(dpy, w, dc.norm[ColBorder]);
  configure(c); /* propagates border_width, if size doesn't change */
  updatewindowtype(c);
  updatesizehints(c);
  updatewmhints(c);
    updateopacity(c);
  c->sfx = c->x;
  c->sfy = c->y;
  c->sfw = c->w;
  c->sfh = c->h;
  XSelectInput(dpy, w, EnterWindowMask|FocusChangeMask|PropertyChangeMask|StructureNotifyMask);
  grabbuttons(c, False);
  if(!c->isfloating)
    c->isfloating = c->oldstate = trans != None || c->isfixed;
  if(c->isfloating)
    XRaiseWindow(dpy, c->win);
  attach(c);
  attachstack(c);
  XMoveResizeWindow(dpy, c->win, c->x + 2 * sw, c->y, c->w, c->h); /* some windows require this */
  setclientstate(c, NormalState);
  if (c->mon == selmon)
    unfocus(selmon->sel, False);
  c->mon->sel = c;
  arrange(c->mon);
  XMapWindow(dpy, c->win);
  focus(NULL);
}

void
mappingnotify(XEvent *e) {
  XMappingEvent *ev = &e->xmapping;

  XRefreshKeyboardMapping(ev);
  if(ev->request == MappingKeyboard)
    grabkeys();
}

void
maprequest(XEvent *e) {
  static XWindowAttributes wa;
  XMapRequestEvent *ev = &e->xmaprequest;

  if(!XGetWindowAttributes(dpy, ev->window, &wa))
    return;
  if(wa.override_redirect)
    return;
  if(!wintoclient(ev->window))
    manage(ev->window, &wa);
}

void
monocle(Monitor *m) {
  unsigned int n = 0;
  Client *c;

  for(c = m->clients; c; c = c->next)
    if(ISVISIBLE(c))
      n++;
  if(n > 0) /* override layout symbol */
    snprintf(m->ltsymbol, sizeof m->ltsymbol, "[%d]", n);
  for(c = nexttiled(m->clients); c; c = nexttiled(c->next))
    resize(c, m->wx-1, m->wy-1, m->ww, m->wh, False);
}

void
motionnotify(XEvent *e) {
  static Monitor *mon = NULL;
  Monitor *m;
  XMotionEvent *ev = &e->xmotion;

  if(ev->window != root)
    return;
  if((m = recttomon(ev->x_root, ev->y_root, 1, 1)) != mon && mon) {
    selmon = m;
    focus(NULL);
  }
  mon = m;
}

void
movemouse(const Arg *arg) {
  int x, y, ocx, ocy, nx, ny;
  Client *c;
  Monitor *m;
  XEvent ev;
  Time lasttime = 0;

  if(!(c = selmon->sel))
    return;
  restack(selmon);
  ocx = c->x;
  ocy = c->y;
  if(XGrabPointer(dpy, root, False, MOUSEMASK, GrabModeAsync, GrabModeAsync,
  None, cursor[CurMove], CurrentTime) != GrabSuccess)
    return;
  if(!getrootptr(&x, &y))
    return;
  do {
    XMaskEvent(dpy, MOUSEMASK|ExposureMask|SubstructureRedirectMask, &ev);
    switch(ev.type) {
    case ConfigureRequest:
    case Expose:
    case MapRequest:
      handler[ev.type](&ev);
      break;
    case MotionNotify:
      if((ev.xmotion.time - lasttime) <= (1000 / 60))
        continue;
      lasttime = ev.xmotion.time;
      nx = ocx + (ev.xmotion.x - x);
      ny = ocy + (ev.xmotion.y - y);
      if(nx >= selmon->wx && nx <= selmon->wx + selmon->ww
      && ny >= selmon->wy && ny <= selmon->wy + selmon->wh) {
        if(abs(selmon->wx - nx) < snap)
          nx = selmon->wx;
        else if(abs((selmon->wx + selmon->ww) - (nx + WIDTH(c))) < snap)
          nx = selmon->wx + selmon->ww - WIDTH(c);
        if(abs(selmon->wy - ny) < snap)
          ny = selmon->wy;
        else if(abs((selmon->wy + selmon->wh) - (ny + HEIGHT(c))) < snap)
          ny = selmon->wy + selmon->wh - HEIGHT(c);
        if(!c->isfloating && selmon->lt[selmon->sellt]->arrange
        && (abs(nx - c->x) > snap || abs(ny - c->y) > snap))
          togglefloating(NULL);
      }
      if(!selmon->lt[selmon->sellt]->arrange || c->isfloating)
        resize(c, nx, ny, c->w, c->h, True);
      break;
    }
  } while(ev.type != ButtonRelease);
  XUngrabPointer(dpy, CurrentTime);
  if((m = recttomon(c->x, c->y, c->w, c->h)) != selmon) {
    sendmon(c, m);
    selmon = m;
    focus(NULL);
  }
}

Client *
nexttiled(Client *c) {
  for(; c && (c->isfloating || !ISVISIBLE(c)); c = c->next);
  return c;
}

void
pop(Client *c) {
  detach(c);
  attach(c);
  focus(c);
  arrange(c->mon);
}

void
propertynotify(XEvent *e) {
  Client *c;
  Window trans;
  XPropertyEvent *ev = &e->xproperty;

  if(ev->state == PropertyDelete)
    return; /* ignore */
  else if((c = wintoclient(ev->window))) {
    switch(ev->atom) {
    default: break;
    case XA_WM_TRANSIENT_FOR:
      if(!c->isfloating && (XGetTransientForHint(dpy, c->win, &trans)) &&
         (c->isfloating = (wintoclient(trans)) != NULL))
        arrange(c->mon);
      break;
    case XA_WM_NORMAL_HINTS:
      updatesizehints(c);
      break;
    case XA_WM_HINTS:
      updatewmhints(c);
      drawbars();
      break;
    }
    if(ev->atom == XA_WM_NAME || ev->atom == netatom[NetWMName]) {
      updatetitle(c);
      if(c == c->mon->sel)
        drawbar(c->mon);
    }
    if(ev->atom == netatom[NetWMWindowType])
      updatewindowtype(c);
  }
}

void
quit(const Arg *arg) {
  running = False;
}

Monitor *
recttomon(int x, int y, int w, int h) {
  Monitor *m, *r = selmon;
  int a, area = 0;

  for(m = mons; m; m = m->next)
    if((a = INTERSECT(x, y, w, h, m)) > area) {
      area = a;
      r = m;
    }
  return r;
}

void
resize(Client *c, int x, int y, int w, int h, Bool interact) {
  if(applysizehints(c, &x, &y, &w, &h, interact))
    resizeclient(c, x, y, w, h);
}

void
resizeclient(Client *c, int x, int y, int w, int h) {
  XWindowChanges wc;

  c->oldx = c->x; c->x = wc.x = x;
  c->oldy = c->y; c->y = wc.y = y;
  c->oldw = c->w; c->w = wc.width = w;
  c->oldh = c->h; c->h = wc.height = h;
  wc.border_width = c->bw;
  XConfigureWindow(dpy, c->win, CWX|CWY|CWWidth|CWHeight|CWBorderWidth, &wc);
  configure(c);
  XSync(dpy, False);
}

void
resizemouse(const Arg *arg) {
  int ocx, ocy, nw, nh;
  Client *c;
  Monitor *m;
  XEvent ev;
  Time lasttime = 0;

  if(!(c = selmon->sel))
    return;
  restack(selmon);
  ocx = c->x;
  ocy = c->y;
  if(XGrabPointer(dpy, root, False, MOUSEMASK, GrabModeAsync, GrabModeAsync,
                  None, cursor[CurResize], CurrentTime) != GrabSuccess)
    return;
  XWarpPointer(dpy, None, c->win, 0, 0, 0, 0, c->w + c->bw - 1, c->h + c->bw - 1);
  do {
    XMaskEvent(dpy, MOUSEMASK|ExposureMask|SubstructureRedirectMask, &ev);
    switch(ev.type) {
    case ConfigureRequest:
    case Expose:
    case MapRequest:
      handler[ev.type](&ev);
      break;
    case MotionNotify:
      if ((ev.xmotion.time - lasttime) <= (1000 / 60))
        continue;
      lasttime = ev.xmotion.time;
      nw = MAX(ev.xmotion.x - ocx - 2 * c->bw + 1, 1);
      nh = MAX(ev.xmotion.y - ocy - 2 * c->bw + 1, 1);
      if(c->mon->wx + nw >= selmon->wx && c->mon->wx + nw <= selmon->wx + selmon->ww
      && c->mon->wy + nh >= selmon->wy && c->mon->wy + nh <= selmon->wy + selmon->wh)
      {
        if(!c->isfloating && selmon->lt[selmon->sellt]->arrange
        && (abs(nw - c->w) > snap || abs(nh - c->h) > snap))
          togglefloating(NULL);
      }
      if(!selmon->lt[selmon->sellt]->arrange || c->isfloating)
        resize(c, c->x, c->y, nw, nh, True);
      break;
    }
  } while(ev.type != ButtonRelease);
  XWarpPointer(dpy, None, c->win, 0, 0, 0, 0, c->w + c->bw - 1, c->h + c->bw - 1);
  XUngrabPointer(dpy, CurrentTime);
  while(XCheckMaskEvent(dpy, EnterWindowMask, &ev));
  if((m = recttomon(c->x, c->y, c->w, c->h)) != selmon) {
    sendmon(c, m);
    selmon = m;
    focus(NULL);
  }
}

void
restack(Monitor *m) {
  Client *c;
  XEvent ev;
  XWindowChanges wc;

  drawbar(m);
  if(!m->sel)
    return;
  if(m->sel->isfloating || !m->lt[m->sellt]->arrange)
    XRaiseWindow(dpy, m->sel->win);
  if(m->lt[m->sellt]->arrange) {
    wc.stack_mode = Below;
    wc.sibling = m->barwin;
    for(c = m->stack; c; c = c->snext)
      if(!c->isfloating && ISVISIBLE(c)) {
        XConfigureWindow(dpy, c->win, CWSibling|CWStackMode, &wc);
        wc.sibling = c->win;
      }
  }
  XSync(dpy, False);
  while(XCheckMaskEvent(dpy, EnterWindowMask, &ev));
}

void
run(void) {
  XEvent ev;
  /* main event loop */
  XSync(dpy, False);
  while(running && !XNextEvent(dpy, &ev)) {
    if(handler[ev.type])
      handler[ev.type](&ev);
    }
}

void
scan(void) {
  unsigned int i, num;
  Window d1, d2, *wins = NULL;
  XWindowAttributes wa;

  if(XQueryTree(dpy, root, &d1, &d2, &wins, &num)) {
    for(i = 0; i < num; i++) {
      if(!XGetWindowAttributes(dpy, wins[i], &wa)
      || wa.override_redirect || XGetTransientForHint(dpy, wins[i], &d1))
        continue;
      if(wa.map_state == IsViewable || getstate(wins[i]) == IconicState)
        manage(wins[i], &wa);
    }
    for(i = 0; i < num; i++) { /* now the transients */
      if(!XGetWindowAttributes(dpy, wins[i], &wa))
        continue;
      if(XGetTransientForHint(dpy, wins[i], &d1)
      && (wa.map_state == IsViewable || getstate(wins[i]) == IconicState))
        manage(wins[i], &wa);
    }
    if(wins)
      XFree(wins);
  }
}

void
sendmon(Client *c, Monitor *m) {
  if(c->mon == m)
    return;
  unfocus(c, True);
  detach(c);
  detachstack(c);
  c->mon = m;
  c->tags = m->tagset[m->seltags]; /* assign tags of target monitor */
  attach(c);
  attachstack(c);
  focus(NULL);
  arrange(NULL);
}

void
setclientstate(Client *c, long state) {
  long data[] = { state, None };

  XChangeProperty(dpy, c->win, wmatom[WMState], wmatom[WMState], 32,
      PropModeReplace, (unsigned char *)data, 2);
}

Bool
sendevent(Client *c, Atom proto) {
  int n;
  Atom *protocols;
  Bool exists = False;
  XEvent ev;

  if(XGetWMProtocols(dpy, c->win, &protocols, &n)) {
    while(!exists && n--)
      exists = protocols[n] == proto;
    XFree(protocols);
  }
  if(exists) {
    ev.type = ClientMessage;
    ev.xclient.window = c->win;
    ev.xclient.message_type = wmatom[WMProtocols];
    ev.xclient.format = 32;
    ev.xclient.data.l[0] = proto;
    ev.xclient.data.l[1] = CurrentTime;
    XSendEvent(dpy, c->win, False, NoEventMask, &ev);
  }
  return exists;
}

void
setfocus(Client *c) {
  if(!c->neverfocus)
    XSetInputFocus(dpy, c->win, RevertToPointerRoot, CurrentTime);
  sendevent(c, wmatom[WMTakeFocus]);
}

void
setfullscreen(Client *c, Bool fullscreen) {
  if(fullscreen && !c->isfullscreen) {
    XChangeProperty(dpy, c->win, netatom[NetWMState], XA_ATOM, 32,
                    PropModeReplace, (unsigned char*)&netatom[NetWMFullscreen], 1);
    c->isfullscreen = True;
    c->oldstate = c->isfloating;
    c->oldbw = c->bw;
    c->bw = 0;
    c->isfloating = True;
    resizeclient(c, c->mon->mx, c->mon->my, c->mon->mw, c->mon->mh);
    XRaiseWindow(dpy, c->win);
  }
  else if (!fullscreen && c->isfullscreen) {
    XChangeProperty(dpy, c->win, netatom[NetWMState], XA_ATOM, 32,
                    PropModeReplace, (unsigned char*)0, 0);
    c->isfullscreen = False;
    c->isfloating = c->oldstate;
    c->bw = c->oldbw;
    c->x = c->oldx;
    c->y = c->oldy;
    c->w = c->oldw;
    c->h = c->oldh;
    resizeclient(c, c->x, c->y, c->w, c->h);
    arrange(c->mon);
  }
}

void
setlayout(const Arg *arg) {
  if(!arg || !arg->v || arg->v != selmon->lt[selmon->sellt]) {
    selmon->pertag->sellts[selmon->pertag->curtag] ^= 1;
    selmon->sellt = selmon->pertag->sellts[selmon->pertag->curtag];
  }
  if(arg && arg->v)
    selmon->pertag->ltidxs[selmon->pertag->curtag][selmon->sellt] = (Layout *)arg->v;
  selmon->lt[selmon->sellt] = selmon->pertag->ltidxs[selmon->pertag->curtag][selmon->sellt];
  strncpy(selmon->ltsymbol, selmon->lt[selmon->sellt]->symbol, sizeof selmon->ltsymbol);
  if(selmon->sel)
    arrange(selmon);
  else
    drawbar(selmon);
}

/* arg > 1.0 will set mfact absolutly */
void
setmfact(const Arg *arg) {
  float f;

  if(!arg || !selmon->lt[selmon->sellt]->arrange)
    return;
  f = arg->f < 1.0 ? arg->f + selmon->mfact : arg->f - 1.0;
  if(f < 0.1 || f > 0.9)
    return;
  selmon->mfact = selmon->pertag->mfacts[selmon->pertag->curtag] = f;
  arrange(selmon);
}

void
setup(void) {
  XSetWindowAttributes wa;

  /* clean up any zombies immediately */
  sigchld(0);
  if(signal(SIGTERM, sigterm) == SIG_ERR)
    die("Can't install SIGTERM handler");

  /* SIGPIPE is being send when a socket connection fails -- workaround
   * for SIGPIPE-related process termination due to a libmpdclient bug */
  if(signal(SIGPIPE, SIG_IGN) == SIG_ERR)
    die("Can't install SIGPIPE handler");

  /* init screen */
  screen = DefaultScreen(dpy);
  root = RootWindow(dpy, screen);
  initfont(font);
  sw = DisplayWidth(dpy, screen);
  sh = DisplayHeight(dpy, screen);
  bh = dc.h = dc.font.height + 3;
  updategeom();
  /* init atoms */
  wmatom[WMProtocols] = XInternAtom(dpy, "WM_PROTOCOLS", False);
  wmatom[WMDelete] = XInternAtom(dpy, "WM_DELETE_WINDOW", False);
  wmatom[WMState] = XInternAtom(dpy, "WM_STATE", False);
  wmatom[WMTakeFocus] = XInternAtom(dpy, "WM_TAKE_FOCUS", False);
  netatom[NetActiveWindow] = XInternAtom(dpy, "_NET_ACTIVE_WINDOW", False);
  netatom[NetSupported] = XInternAtom(dpy, "_NET_SUPPORTED", False);
  netatom[NetWMName] = XInternAtom(dpy, "_NET_WM_NAME", False);
  netatom[NetWMState] = XInternAtom(dpy, "_NET_WM_STATE", False);
  netatom[NetWMFullscreen] = XInternAtom(dpy, "_NET_WM_STATE_FULLSCREEN", False);
  netatom[NetWMWindowType] = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE", False);
  netatom[NetWMWindowTypeDialog] = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_DIALOG", False);
  /* init cursors */
  cursor[CurNormal] = XCreateFontCursor(dpy, XC_left_ptr);
  cursor[CurResize] = XCreateFontCursor(dpy, XC_sizing);
  cursor[CurMove] = XCreateFontCursor(dpy, XC_fleur);

  /* init appearance */
        dc.norm[ColBorder] = getcolor(normbordercolor, dc.xftnorm+ColBorder);
        dc.norm[ColBG] = getcolor(normbgcolor, dc.xftnorm+ColBG);
        dc.norm[ColFG] = getcolor(normfgcolor, dc.xftnorm+ColFG);
        dc.sel[ColBorder] = getcolor(selbordercolor, dc.xftsel+ColBorder);
        dc.sel[ColBG] = getcolor(selbgcolor, dc.xftsel+ColBG);
        dc.sel[ColFG] = getcolor(selfgcolor, dc.xftsel+ColFG);

  dc.drawable = XCreatePixmap(dpy, root, DisplayWidth(dpy, screen), bh, DefaultDepth(dpy, screen));
  dc.gc = XCreateGC(dpy, root, 0, NULL);
  XSetLineAttributes(dpy, dc.gc, 1, LineSolid, CapButt, JoinMiter);

        dc.xftdrawable = XftDrawCreate(dpy, dc.drawable, DefaultVisual(dpy,screen), DefaultColormap(dpy,screen));
        if(!dc.xftdrawable)
          printf("error, cannot create drawable\n");

  /* init bar */
  updatebars();
  /* init mpdcmd functionality */
  mpdcmd_init();
  /* EWMH support per view */
  XChangeProperty(dpy, root, netatom[NetSupported], XA_ATOM, 32,
      PropModeReplace, (unsigned char *) netatom, NetLast);
  /* select for events */
  wa.cursor = cursor[CurNormal];
  wa.event_mask = SubstructureRedirectMask|SubstructureNotifyMask|ButtonPressMask|PointerMotionMask
                  |EnterWindowMask|LeaveWindowMask|StructureNotifyMask|PropertyChangeMask;
  XChangeWindowAttributes(dpy, root, CWEventMask|CWCursor, &wa);
  XSelectInput(dpy, root, wa.event_mask);
  grabkeys();
  focus(NULL);
}

void
showhide(Client *c) {
  if(!c)
    return;
  if(ISVISIBLE(c)) { /* show clients top down */
    XMoveWindow(dpy, c->win, c->x, c->y);
    if((!c->mon->lt[c->mon->sellt]->arrange || c->isfloating) && !c->isfullscreen)
      resize(c, c->x, c->y, c->w, c->h, False);
    showhide(c->snext);
  }
  else { /* hide clients bottom up */
    showhide(c->snext);
    XMoveWindow(dpy, c->win, WIDTH(c) * -2, c->y);
  }
}

void
sigchld(int unused) {
  if(signal(SIGCHLD, sigchld) == SIG_ERR)
    die("Can't install SIGCHLD handler");
  while(0 < waitpid(-1, NULL, WNOHANG));
}

void
sigterm(int unused) {
  LERROR(0, 0, "caught SIGTERM");
  running = False;
}

void
spawn(const Arg *arg) {
  if(fork() == 0) {
    if(dpy)
      close(ConnectionNumber(dpy));
    setsid();
    execvp(((char **)arg->v)[0], (char **)arg->v);
    fprintf(stderr, "dwm: execvp %s", ((char **)arg->v)[0]);
    perror(" failed");
    exit(EXIT_SUCCESS);
  }
}

void
tag(const Arg *arg) {
  if(selmon->sel && arg->ui & TAGMASK) {
    selmon->sel->tags = arg->ui & TAGMASK;
    focus(NULL);
    arrange(selmon);
  }
}

void
tagmon(const Arg *arg) {
  if(!selmon->sel || !mons->next)
    return;
  sendmon(selmon->sel, dirtomon(arg->i));
}

int
textnw(const char *text, unsigned int len) {
  PangoRectangle r;
  pango_layout_set_text(dc.plo, text, len);
  pango_layout_get_extents(dc.plo, &r, 0);
  return r.width / PANGO_SCALE;
}

void
deck(Monitor *m) {
  int dn;
  unsigned int i, n, h, mw, my;
  Client *c;

  for(n = 0, c = nexttiled(m->clients); c; c = nexttiled(c->next), n++);
  if(n == 0)
    return;

  dn = n - m->nmaster;
  if(dn > 0) /* override layout symbol */
    snprintf(m->ltsymbol, sizeof m->ltsymbol, "D %d", dn);

  if(n > m->nmaster)
    mw = m->nmaster ? m->ww * m->mfact : 0;
  else
    mw = m->ww;
  for(i = my = 0, c = nexttiled(m->clients); c; c = nexttiled(c->next), i++)
    if(i < m->nmaster) {
      h = (m->wh - my) / (MIN(n, m->nmaster) - i);
      resize(c, m->wx, m->wy + my, mw - (2*c->bw), h - (2*c->bw), False);
      my += HEIGHT(c);
    }
    else
      resize(c, m->wx + mw, m->wy, m->ww - mw - (2*c->bw), m->wh - (2*c->bw), False);
}


void
tile(Monitor *m) {
  unsigned int i, n, h, mw, my, ty;
  float mfacts = 0, sfacts = 0;
  Client *c;

  for(n = 0, c = nexttiled(m->clients); c; c = nexttiled(c->next), n++) {
    if(n < m->nmaster)
      mfacts += c->cfact;
    else
      sfacts += c->cfact;
  }
  if(n == 0)
    return;

  if(n > m->nmaster)
    mw = m->nmaster ? m->ww * m->mfact : 0;
  else
    mw = m->ww;
  for(i = my = ty = 0, c = nexttiled(m->clients); c; c = nexttiled(c->next), i++)
    if(i < m->nmaster) {
      h = (m->wh - my) * (c->cfact / mfacts);
      resize(c, m->wx, m->wy + my, mw - (2*c->bw), h - (2*c->bw), False);
      my += HEIGHT(c);
      mfacts -= c->cfact;
    }
    else {
      h = (m->wh - ty) / (n - i);
      resize(c, m->wx + mw, m->wy + ty, m->ww - mw - (2*c->bw), h - (2*c->bw), False);
      ty += HEIGHT(c);
      sfacts -= c->cfact;
    }
}

void
togglebar(const Arg *arg) {
  selmon->showbar = selmon->pertag->showbars[selmon->pertag->curtag] = !selmon->showbar;
  updatebarpos(selmon);
  XMoveResizeWindow(dpy, selmon->barwin, selmon->wx, selmon->by, selmon->ww, bh);
  arrange(selmon);
}

void
togglefloating(const Arg *arg) {
  if(!selmon->sel)
    return;
  selmon->sel->isfloating = !selmon->sel->isfloating || selmon->sel->isfixed;
  if(selmon->sel->isfloating)
    /*restore last known float dimensions*/
    resize(selmon->sel, selmon->sel->sfx, selmon->sel->sfy,
           selmon->sel->sfw, selmon->sel->sfh, False);
  else {
    /*save last known float dimensions*/
    selmon->sel->sfx = selmon->sel->x;
    selmon->sel->sfy = selmon->sel->y;
    selmon->sel->sfw = selmon->sel->w;
    selmon->sel->sfh = selmon->sel->h;
  }
  arrange(selmon);
}

void
toggletag(const Arg *arg) {
  unsigned int newtags;

  if(!selmon->sel)
    return;
  newtags = selmon->sel->tags ^ (arg->ui & TAGMASK);
  if(newtags) {
    selmon->sel->tags = newtags;
    focus(NULL);
    arrange(selmon);
  }
}

void
toggleview(const Arg *arg) {
  unsigned int newtagset = selmon->tagset[selmon->seltags] ^ (arg->ui & TAGMASK);
  int i;

  if(newtagset) {
    if(newtagset == ~0) {
      selmon->pertag->prevtag = selmon->pertag->curtag;
      selmon->pertag->curtag = 0;
    }
    /* test if the user did not select the same tag */
    if(!(newtagset & 1 << (selmon->pertag->curtag - 1))) {
      selmon->pertag->prevtag = selmon->pertag->curtag;
      for (i=0; !(newtagset & 1 << i); i++) ;
      selmon->pertag->curtag = i + 1;
    }
    selmon->tagset[selmon->seltags] = newtagset;

    /* apply settings for this view */
    selmon->nmaster = selmon->pertag->nmasters[selmon->pertag->curtag];
    selmon->mfact = selmon->pertag->mfacts[selmon->pertag->curtag];
    selmon->sellt = selmon->pertag->sellts[selmon->pertag->curtag];
    selmon->lt[selmon->sellt] = selmon->pertag->ltidxs[selmon->pertag->curtag][selmon->sellt];
    selmon->lt[selmon->sellt^1] = selmon->pertag->ltidxs[selmon->pertag->curtag][selmon->sellt^1];
    if (selmon->showbar != selmon->pertag->showbars[selmon->pertag->curtag])
      togglebar(NULL);
    focus(NULL);
    arrange(selmon);
  }
}

void
unfocus(Client *c, Bool setfocus) {
  if(!c)
    return;
  grabbuttons(c, False);
  XSetWindowBorder(dpy, c->win, dc.norm[ColBorder]);
  if(setfocus)
    XSetInputFocus(dpy, root, RevertToPointerRoot, CurrentTime);
}

void
unmanage(Client *c, Bool destroyed) {
  Monitor *m = c->mon;
  XWindowChanges wc;

  /* The server grab construct avoids race conditions. */
  detach(c);
  detachstack(c);
  if(!destroyed) {
    wc.border_width = c->oldbw;
    XGrabServer(dpy);
    XSetErrorHandler(xerrordummy);
    XConfigureWindow(dpy, c->win, CWBorderWidth, &wc); /* restore border */
    XUngrabButton(dpy, AnyButton, AnyModifier, c->win);
    setclientstate(c, WithdrawnState);
    XSync(dpy, False);
    XSetErrorHandler(xerror);
    XUngrabServer(dpy);
  }
  free(c);
  focus(NULL);
  arrange(m);
}

void
unmapnotify(XEvent *e) {
  Client *c;
  XUnmapEvent *ev = &e->xunmap;

  if((c = wintoclient(ev->window))) {
    if(ev->send_event)
      setclientstate(c, WithdrawnState);
    else
      unmanage(c, False);
  }
}

void
updatebars(void) {
  Monitor *m;
  XSetWindowAttributes wa = {
    .override_redirect = True,
    .background_pixmap = ParentRelative,
    .event_mask = ButtonPressMask|ExposureMask
  };
  for(m = mons; m; m = m->next) {
    m->barwin = XCreateWindow(dpy, root, m->wx, m->by, m->ww, bh, 0, DefaultDepth(dpy, screen),
                              CopyFromParent, DefaultVisual(dpy, screen),
                              CWOverrideRedirect|CWBackPixmap|CWEventMask, &wa);
    XDefineCursor(dpy, m->barwin, cursor[CurNormal]);
    XMapRaised(dpy, m->barwin);
  }
}

void
updatebarpos(Monitor *m) {
  m->wy = m->my;
  m->wh = m->mh;
  if(m->showbar) {
    m->wh -= bh;
    m->by = m->topbar ? m->wy : m->wy + m->wh;
    m->wy = m->topbar ? m->wy + bh : m->wy;
  }
  else
    m->by = -bh;
}

Bool
updategeom(void) {
  Bool dirty = False;

#ifdef XINERAMA
  if(XineramaIsActive(dpy)) {
    int i, j, n, nn;
    Client *c;
    Monitor *m;
    XineramaScreenInfo *info = XineramaQueryScreens(dpy, &nn);
    XineramaScreenInfo *unique = NULL;

    for(n = 0, m = mons; m; m = m->next, n++);
    /* only consider unique geometries as separate screens */
    if(!(unique = (XineramaScreenInfo *)malloc(sizeof(XineramaScreenInfo) * nn)))
      die("fatal: could not malloc() %u bytes\n", sizeof(XineramaScreenInfo) * nn);
    for(i = 0, j = 0; i < nn; i++)
      if(isuniquegeom(unique, j, &info[i]))
        memcpy(&unique[j++], &info[i], sizeof(XineramaScreenInfo));
    XFree(info);
    nn = j;
    if(n <= nn) {
      for(i = 0; i < (nn - n); i++) { /* new monitors available */
        for(m = mons; m && m->next; m = m->next);
        if(m)
          m->next = createmon();
        else
          mons = createmon();
      }
      for(i = 0, m = mons; i < nn && m; m = m->next, i++)
        if(i >= n
        || (unique[i].x_org != m->mx || unique[i].y_org != m->my
            || unique[i].width != m->mw || unique[i].height != m->mh))
        {
          dirty = True;
          m->num = i;
          m->mx = m->wx = unique[i].x_org;
          m->my = m->wy = unique[i].y_org;
          m->mw = m->ww = unique[i].width;
          m->mh = m->wh = unique[i].height;
          updatebarpos(m);
        }
    }
    else { /* less monitors available nn < n */
      for(i = nn; i < n; i++) {
        for(m = mons; m && m->next; m = m->next);
        while(m->clients) {
          dirty = True;
          c = m->clients;
          m->clients = c->next;
          detachstack(c);
          c->mon = mons;
          attach(c);
          attachstack(c);
        }
        if(m == selmon)
          selmon = mons;
        cleanupmon(m);
      }
    }
    free(unique);
  }
  else
#endif /* XINERAMA */
  /* default monitor setup */
  {
    if(!mons)
      mons = createmon();
    if(mons->mw != sw || mons->mh != sh) {
      dirty = True;
      mons->mw = mons->ww = sw;
      mons->mh = mons->wh = sh;
      updatebarpos(mons);
    }
  }
  if(dirty) {
    selmon = mons;
    selmon = wintomon(root);
  }
  return dirty;
}

void
updatenumlockmask(void) {
  unsigned int i, j;
  XModifierKeymap *modmap;

  numlockmask = 0;
  modmap = XGetModifierMapping(dpy);
  for(i = 0; i < 8; i++)
    for(j = 0; j < modmap->max_keypermod; j++)
      if(modmap->modifiermap[i * modmap->max_keypermod + j]
         == XKeysymToKeycode(dpy, XK_Num_Lock))
        numlockmask = (1 << i);
  XFreeModifiermap(modmap);
}

void
updatesizehints(Client *c) {
  long msize;
  XSizeHints size;

  if(!XGetWMNormalHints(dpy, c->win, &size, &msize))
    /* size is uninitialized, ensure that size.flags aren't used */
    size.flags = PSize;
  if(size.flags & PBaseSize) {
    c->basew = size.base_width;
    c->baseh = size.base_height;
  }
  else if(size.flags & PMinSize) {
    c->basew = size.min_width;
    c->baseh = size.min_height;
  }
  else
    c->basew = c->baseh = 0;
  if(size.flags & PResizeInc) {
    c->incw = size.width_inc;
    c->inch = size.height_inc;
  }
  else
    c->incw = c->inch = 0;
  if(size.flags & PMaxSize) {
    c->maxw = size.max_width;
    c->maxh = size.max_height;
  }
  else
    c->maxw = c->maxh = 0;
  if(size.flags & PMinSize) {
    c->minw = size.min_width;
    c->minh = size.min_height;
  }
  else if(size.flags & PBaseSize) {
    c->minw = size.base_width;
    c->minh = size.base_height;
  }
  else
    c->minw = c->minh = 0;
  if(size.flags & PAspect) {
    c->mina = (float)size.min_aspect.y / size.min_aspect.x;
    c->maxa = (float)size.max_aspect.x / size.max_aspect.y;
  }
  else
    c->maxa = c->mina = 0.0;
  c->isfixed = (c->maxw && c->minw && c->maxh && c->minh
               && c->maxw == c->minw && c->maxh == c->minh);
}

void
updatetitle(Client *c) {
  if(!gettextprop(c->win, netatom[NetWMName], c->name, sizeof c->name))
    gettextprop(c->win, XA_WM_NAME, c->name, sizeof c->name);
  if(c->name[0] == '\0') /* hack to mark broken clients */
    strcpy(c->name, broken);
}


void
updatewindowtype(Client *c) {
  Atom state = getatomprop(c, netatom[NetWMState]);
  Atom wtype = getatomprop(c, netatom[NetWMWindowType]);

  if(state == netatom[NetWMFullscreen])
    setfullscreen(c, True);

  if(wtype == netatom[NetWMWindowTypeDialog])
    c->isfloating = True;
}

void
updatewmhints(Client *c) {
  XWMHints *wmh;

  if((wmh = XGetWMHints(dpy, c->win))) {
    if(c == selmon->sel && wmh->flags & XUrgencyHint) {
      wmh->flags &= ~XUrgencyHint;
      XSetWMHints(dpy, c->win, wmh);
    }
    else
      c->isurgent = (wmh->flags & XUrgencyHint) ? True : False;
    if(wmh->flags & InputHint)
      c->neverfocus = !wmh->input;
    else
      c->neverfocus = False;
    XFree(wmh);
  }
}

void
view(const Arg *arg) {
  int i;
  unsigned int tmptag;

  if((arg->ui & TAGMASK) == selmon->tagset[selmon->seltags])
    return;
  selmon->seltags ^= 1; /* toggle sel tagset */
  if(arg->ui & TAGMASK) {
    selmon->pertag->prevtag = selmon->pertag->curtag;
    selmon->tagset[selmon->seltags] = arg->ui & TAGMASK;
    if(arg->ui == ~0)
      selmon->pertag->curtag = 0;
    else {
      for (i=0; !(arg->ui & 1 << i); i++) ;
      selmon->pertag->curtag = i + 1;
    }
  } else {
    tmptag = selmon->pertag->prevtag;
    selmon->pertag->prevtag = selmon->pertag->curtag;
    selmon->pertag->curtag = tmptag;
  }
  selmon->nmaster = selmon->pertag->nmasters[selmon->pertag->curtag];
  selmon->mfact = selmon->pertag->mfacts[selmon->pertag->curtag];
  selmon->sellt = selmon->pertag->sellts[selmon->pertag->curtag];
  selmon->lt[selmon->sellt] = selmon->pertag->ltidxs[selmon->pertag->curtag][selmon->sellt];
  selmon->lt[selmon->sellt^1] = selmon->pertag->ltidxs[selmon->pertag->curtag][selmon->sellt^1];
  if (selmon->showbar != selmon->pertag->showbars[selmon->pertag->curtag])
    togglebar(NULL);
  focus(NULL);
  arrange(selmon);
}

Client *
wintoclient(Window w) {
  Client *c;
  Monitor *m;

  for(m = mons; m; m = m->next)
    for(c = m->clients; c; c = c->next)
      if(c->win == w)
        return c;
  return NULL;
}

Monitor *
wintomon(Window w) {
  int x, y;
  Client *c;
  Monitor *m;

  if(w == root && getrootptr(&x, &y))
    return recttomon(x, y, 1, 1);
  for(m = mons; m; m = m->next)
    if(w == m->barwin)
      return m;
  if((c = wintoclient(w)))
    return c->mon;
  return selmon;
}

/* There's no way to check accesses to destroyed windows, thus those cases are
 * ignored (especially on UnmapNotify's).  Other types of errors call Xlibs
 * default error handler, which may call exit.  */
int
xerror(Display *dpy, XErrorEvent *ee) {
  if(ee->error_code == BadWindow
  || (ee->request_code == X_SetInputFocus && ee->error_code == BadMatch)
  || (ee->request_code == X_PolyText8 && ee->error_code == BadDrawable)
  || (ee->request_code == X_PolyFillRectangle && ee->error_code == BadDrawable)
  || (ee->request_code == X_PolySegment && ee->error_code == BadDrawable)
  || (ee->request_code == X_ConfigureWindow && ee->error_code == BadMatch)
  || (ee->request_code == X_GrabButton && ee->error_code == BadAccess)
  || (ee->request_code == X_GrabKey && ee->error_code == BadAccess)
  || (ee->request_code == X_CopyArea && ee->error_code == BadDrawable))
    return 0;
  fprintf(stderr, "dwm: fatal error: request code=%d, error code=%d\n",
      ee->request_code, ee->error_code);
  return xerrorxlib(dpy, ee); /* may call exit */
}

int
xerrordummy(Display *dpy, XErrorEvent *ee) {
  return 0;
}

/* Startup Error handler to check if another window manager
 * is already running. */
int
xerrorstart(Display *dpy, XErrorEvent *ee) {
  die("dwm: another window manager is already running\n");
  return -1;
}

void
zoom(const Arg *arg) {
  Client *c = selmon->sel;

  if(!selmon->lt[selmon->sellt]->arrange
  || (selmon->sel && selmon->sel->isfloating))
    return;
  if(c == nexttiled(selmon->clients))
    if(!c || !(c = nexttiled(c->next)))
      return;
  pop(c);
}

int
shifttag(int dist) {
  int i, curtags;
  int seltag = 0;
  int numtags = LENGTH(tags);

  curtags = selmon->tagset[selmon->seltags];
  for(i = 0; i < LENGTH(tags); i++) {
    if((curtags & (1 << i)) != 0) {
      seltag = i;
      break;
    }
  }

  seltag += dist;
  if(seltag < 0)
    seltag = numtags - (-seltag) % numtags;
  else
    seltag %= numtags;

  return 1 << seltag;
}

void
cycle(const Arg *arg) {
  const Arg a = { .i = shifttag(arg->i) };
  view(&a);
}

void
tagcycle(const Arg *arg) {
  const Arg a = { .i = shifttag(arg->i) };
  tag(&a);
  view(&a);
}

void
mpdcmd_toggle(struct mpd_connection *c,
        bool (*statf)(const struct mpd_status*),
        bool (*setf)(struct mpd_connection*, bool) ) {
    struct mpd_status *s;
    if((s = mpd_run_status(c)) == NULL)
        return;
    setf(c, (statf(s) == 1) ? 0 : 1);
    mpd_status_free(s);
}

int
mpdcmd_connect(void) {
  int retries = cfg_mpdcmd_retries;
  do {
    retries -= 1;
    if(mpdc == NULL)
      if((mpdc = mpd_connection_new(cfg_mpdcmd_mpdhost, cfg_mpdcmd_mpdport, 0)) == NULL) {
          LERROR(0,0, "connection attempt %d (of %d) failed",
              retries, cfg_mpdcmd_retries);
          continue;
      }
    if(mpd_connection_get_error(mpdc) != MPD_ERROR_SUCCESS) {
      LERROR(0,0, "mpd: %s",
          mpd_connection_get_error_message(mpdc));
      mpd_connection_free(mpdc);
      mpdc = NULL;
      continue;
    } else
      break;
  } while(retries >= 0);
  if(mpdc == NULL)
    return 1;
  LERROR(0, 0, "mpd: Connected");
  return 0;
}

void
mpdcmd_init_registers(void)
{
    int i;
    const char pn[] = "dwm-mpdcmd-%d";
    MPDCMD_BE_CONNECTED;
    for(i = 0; i < 10; i++)
        sprintf(MpdCmdRegisterPlaylists[i], pn, i);
}

void
mpdcmd_cleanup(void)
{
    int i;
    MPDCMD_BE_CONNECTED;
    for(i = 0; i < 10; i++)
        if(MpdcmdRegister[i][0] == 1)
            mpd_run_rm(mpdc, MpdCmdRegisterPlaylists[i]);
    if(mpdc != NULL)
        mpd_connection_free(mpdc);
    if(MpdcmdCanNotify == 1)
      notify_uninit();
}

void
mpdcmd_savepos(const Arg *arg)
{
    struct mpd_status *s;
    enum mpd_state st;
    int reg = arg->i;

    if(reg < 0 || reg > 9)
        return;
    MPDCMD_BE_CONNECTED;
    if((s = mpd_run_status(mpdc)) == NULL)
        return;
    st = mpd_status_get_state(s);
    if(st == MPD_STATE_STOP || st == MPD_STATE_UNKNOWN) {
        mpd_status_free(s);
        return;
    }
    if(MpdcmdRegister[reg][0] == 1)
        mpd_run_rm(mpdc, MpdCmdRegisterPlaylists[reg]);
    else
        MpdcmdRegister[reg][0] = 1;
    MpdcmdRegister[reg][2] = mpd_status_get_song_pos(s);
    MpdcmdRegister[reg][3] = (int) mpd_status_get_elapsed_time(s);
    mpd_status_free(s);
    mpd_run_save(mpdc, MpdCmdRegisterPlaylists[reg]);
}

void
mpdcmd_loadpos(const Arg *arg) {
  int reg = arg->i;
  if(reg < 0 || reg > 9 || MpdcmdRegister[reg][0] != 1)
      return;
  MPDCMD_BE_CONNECTED;
  mpd_run_clear(mpdc);
  mpd_run_load(mpdc, MpdCmdRegisterPlaylists[reg]);
  if(mpd_run_play_pos(mpdc, (unsigned) MpdcmdRegister[reg][2]))
    mpd_run_seek_pos(mpdc,
        (unsigned) MpdcmdRegister[reg][2],
        (unsigned) MpdcmdRegister[reg][3]);
  return;
};

void
mpdcmd_volume(const Arg *arg) {
  struct mpd_status *s = mpd_run_status(mpdc);
  int vol = 0;
  if(s == NULL) return;
  vol = mpd_status_get_volume(s);
  switch(arg->i) {
    case MpdRaiseVolume:
      vol += voldelta;
      if(vol > 100) vol = 100;
      break;
    case MpdLowerVolume:
      vol -= voldelta;
      if(vol < 0) vol = 0;
      break;
    case MpdMuteVolume:
      spawn(&(Arg){ .v = cfg_mpdcmd_mute_command });
      mpdcmd_notify_volume();
      break;
  }
  mpd_run_set_volume(mpdc, vol);
  mpd_status_free(s);
}

void
mpdcmd_toggle_pause(void) {
  struct mpd_status *s = mpd_run_status(mpdc);
  if(s == NULL) return;
  if(mpd_status_get_state(s) == MPD_STATE_PLAY)
    mpd_run_pause(mpdc, true);
  else
    mpd_run_play(mpdc);
  mpd_status_free(s);
}

int
mpdcmd_eval_forceflag(int value, int flag)
{
  switch(flag) {
    case MpdFlag_Config_Respect:
      return value;
    case MpdFlag_Config_ForceOff:
      return 0;
    case MpdFlag_Config_ForceOn:
      return 1;
  }
  return value;
}


void
mpdcmd_prevnext(int which, int override_notify) { MPDCMD_BE_CONNECTED;
  switch(which) {
    case MpdNext:
      mpd_run_next(mpdc);
      break;
    case MpdPrev:
      mpd_run_previous(mpdc);
      break;
  }
  if( /* Notifcations enabled, watcher not running */
      mpdcmd_eval_forceflag(cfg_mpdcmd_notify_enable, override_notify) == 1 && mpd_watcher_exists == 0)
    mpdcmd_prevnext_notify(which);
}

void
mpdcmd_prevnext_notify(int which) { MPDCMD_BE_CONNECTED; (void) which;
  MpdcmdNotification n;
  const char *song_title = "ー";
  const char *song_artist = "ー";
  const char *song_album = "ー";
  const char *r = NULL;
  int song_pos = 0;
  int song_listlen = 0;
  int song_totaltime = 0;
  int song_minutes = 0;
  int song_seconds = 0;
  enum mpd_state st;
  struct mpd_status *s = NULL;
  struct mpd_song *so = NULL;
  // prepare notification
  if((s = mpd_run_status(mpdc)) == NULL)
      return;
  st = mpd_status_get_state(s);
  if(st == MPD_STATE_STOP || st == MPD_STATE_UNKNOWN)
    goto cleanup;
  if((so = mpd_run_current_song(mpdc)) == NULL)
    goto cleanup;
  if((r = mpd_song_get_tag((const struct mpd_song*)so,
      MPD_TAG_TITLE, 0)) != NULL)
    song_title = r;
  if((r = mpd_song_get_tag((const struct mpd_song*)so,
      MPD_TAG_ARTIST, 0)) != NULL)
    song_artist = r;
  if((r = mpd_song_get_tag((const struct mpd_song*)so,
      MPD_TAG_ALBUM, 0)) != NULL)
    song_album = r;
  song_pos = mpd_status_get_song_pos(s) + 1;
  song_listlen = mpd_status_get_queue_length(s);
  song_totaltime = mpd_status_get_total_time(s);
  song_seconds = song_totaltime % 60;
  song_minutes = (song_totaltime - song_seconds) / 60;
  mpdcmd_notify_settitle(&n, song_artist, song_title);
  mpdcmd_notify_settext(&n, song_album, song_pos, song_listlen,
      song_minutes, song_seconds);
  mpdcmd_notify(&n);
  mpdcmd_free_notification(&n);
cleanup:
  mpd_status_free(s);
  if(so != NULL)
    mpd_song_free(so);
}

void
mpdcmd_notify_statusflags(void) { MPDCMD_BE_CONNECTED;
  struct mpd_status *st = mpd_run_status(mpdc);
  if(st == NULL) return;
  char mask[8] = {
                                          '[',
    mpd_status_get_repeat(st)     ? 'r' : '-',
    mpd_status_get_random(st)     ? 'z' : '-',
    mpd_status_get_single(st)     ? 's' : '-',
    mpd_status_get_consume(st)    ? 'c' : '-',
    mpd_status_get_crossfade(st)  ? 'x' : '-',
                                          ']',
                                          '\0'
  };
  mpdcmd_notify(&(MpdcmdNotification){ mask, NULL });
  mpd_status_free(st);
}

void
mpdcmd_notify_volume(void) { MPDCMD_BE_CONNECTED;
  char txt[14]; /* %d will at most be 100, at least -1 */
  struct mpd_status *st = mpd_run_status(mpdc);
  if(st == NULL) return;
  snprintf(txt, sizeof(txt), "Volume: %d", mpd_status_get_volume(st));
  mpdcmd_notify(&(MpdcmdNotification){ txt, NULL });
  mpd_status_free(st);
}

void
mpdcmd(const Arg *arg) {
  MPDCMD_BE_CONNECTED;
  switch(arg->i) {
    case MpdNotifySong:
      mpdcmd_prevnext_notify(0);
      break;
    case MpdNotifyStatus:
      mpdcmd_notify_statusflags();
      break;
    case MpdNotifyVolume:
      mpdcmd_notify_volume();
      break;
    case MpdTogglePause:
      mpdcmd_toggle_pause();
      break;
    case MpdPrev:
      mpdcmd_prevnext(MpdPrev, MpdFlag_Config_Respect);
      break;
    case MpdNext:
      mpdcmd_prevnext(MpdNext, MpdFlag_Config_Respect);
      break;
    case MpdPlayAgain:
      mpdcmd_prevnext(MpdPrev, MpdFlag_Config_ForceOff);
      mpdcmd_prevnext(MpdNext, MpdFlag_Config_Respect);
      break;
    case MpdRaiseVolume:
    case MpdLowerVolume:
    case MpdMuteVolume:
      mpdcmd_volume(arg);
      break;
    case MpdToggleRepeat:
      mpdcmd_toggle(mpdc, mpd_status_get_repeat, mpd_run_repeat);
      mpdcmd_notify_statusflags();
      break;
    case MpdToggleConsume:
      mpdcmd_toggle(mpdc, mpd_status_get_consume, mpd_run_consume);
      mpdcmd_notify_statusflags();
      break;
    case MpdToggleRandom:
      mpdcmd_toggle(mpdc, mpd_status_get_random, mpd_run_random);
      mpdcmd_notify_statusflags();
      break;
    case MpdToggleSingle:
      mpdcmd_toggle(mpdc, mpd_status_get_single, mpd_run_single);
      mpdcmd_notify_statusflags();
      break;
    case MpdToggleWatcher:
      if(mpd_watcher_exists == 0)
        mpdcmd_start_watcher();
      if(mpd_watcher_exists == 1)
        mpdcmd_stop_watcher();
      break;
    case MpdUpdate:
      mpd_run_update(mpdc, NULL);
      break;
  }
}

void
mpdcmd_init(void) {
  mpdcmd_init_registers();
  mpdcmd_init_notify();
}

void
mpdcmd_init_notify(void) {
  if(cfg_mpdcmd_notify_enable == 0)
    return;
  if(notify_init(cfg_mpdcmd_notify_clientid))
    MpdcmdCanNotify = 1;
}

void
mpdcmd_notify_settitle(MpdcmdNotification *n, const char *artist, const char *title) {
  assert(n != NULL);
  assert(artist != NULL);
  assert(title != NULL);
  char *msg = NULL;
  const char *fmt = "%s 〜 %s";
  int msglen = snprintf(NULL, 0, fmt, artist, title) + 1;
  msg = malloc(sizeof(wchar_t)*(msglen));
  assert(msg != NULL);
  snprintf(msg, msglen, fmt, artist, title);
  n->title = msg;
}

void
mpdcmd_notify_settext(MpdcmdNotification *n, const char *album, int pos, int queuelen, int minutes, int seconds) {
  assert(n != NULL);
  assert(album != NULL);
  const char *fmt = "%s · #%d/%d · %d:%02d";
  char *msg = NULL;
  int msglen = snprintf(NULL, 0, fmt, album, pos, queuelen, minutes,
      seconds) + 1;
  msg = malloc(sizeof(wchar_t)*(msglen));
  assert(msg != NULL);
  snprintf(msg, msglen, fmt, album, pos, queuelen, minutes, seconds);
  n->txt = msg;
}

void
mpdcmd_notify(const MpdcmdNotification *n) {
  assert(n != NULL);
  NotifyNotification *nn = NULL;
  GError *er = NULL;
  int  rc = cfg_mpdcmd_notify_retries;
  // recover an eventually lost connection
  while(MpdcmdCanNotify != 1 && --rc > 0)
    mpdcmd_init_notify();
  nn = notify_notification_new(n->title, n->txt, NULL);
  assert(nn != NULL);
  notify_notification_set_timeout(nn, cfg_mpdcmd_notify_timeout*1000);
  notify_notification_show(nn, &er);
  //FIXME: is this method to free nn the right one?
  // free(nn); <- THIS is NOT right. fucking retarded notification interface.
}

void
mpdcmd_free_notification(MpdcmdNotification *n) {
  assert(n != NULL);
  if(n->title != NULL) free(n->title);
  if(n->txt != NULL) free(n->txt);
}

void
mpdcmd_start_watcher(void)
{
  pthread_attr_t attr;

  pthread_attr_init(&attr);
  if(pthread_create(&mpd_watcher_thread, &attr, mpdcmd_watcher, NULL) != 0)
    LERROR(0, errno, "pthread_create()");
  pthread_attr_destroy(&attr);
  mpd_watcher_exists = 1;
}

void
mpdcmd_stop_watcher(void)
{
  pthread_cancel(mpd_watcher_thread);
  mpd_watcher_exists = 0;
}

void*
mpdcmd_watcher(void *arg)
{
  struct mpd_connection *con = NULL;
  struct mpd_status *st = NULL;
  int psid = -1;
  int csid = -1;
  int bcnt = 3;

  (void) arg;

  for(;;)
  {
    usleep(cfg_mpdcmd_watch_interval);

    if(con == NULL || mpd_connection_get_error(con) != MPD_ERROR_SUCCESS)
    {
      if(--bcnt < 0)
      {
        LERROR(0, 0, "all connection attempts failed, stopping the watcher");
        break;
      }
      LERROR(0, 0, "no connection, calling mpd_connection_new() ...");
      con = mpd_connection_new(cfg_mpdcmd_mpdhost, cfg_mpdcmd_mpdport, 0);
      continue;
    }

    if((st = mpd_run_status(con)) == NULL)
      continue;

    csid = mpd_status_get_song_id(st);

    if(csid != psid)
    {
      mpdcmd_prevnext_notify(0); 
      psid = csid;
    }

    mpd_status_free(st);
  }

  if(con != NULL)
    mpd_connection_free(con);

  return NULL;
}

void
mpvcmd(const Arg *a)
{
  int sfd;
  struct sockaddr_un addr;

  if((sfd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1)
    return;
  memset(&addr, 0, sizeof(struct sockaddr_un));
  addr.sun_family = AF_UNIX;
  strncpy(addr.sun_path, mpvsocket, sizeof(addr.sun_path)-1);

  if(connect(sfd, (struct sockaddr*)&addr, sizeof(struct sockaddr_un)) == -1)
  {
    close(sfd);
    return;
  }

/* When defining additional actions, note that every command must be
 * terminated by a newline '\n' character.  */
#define mpv_send(msg) \
  {\
    int tmp __attribute__((unused)) = write(sfd, msg, sizeof(msg)-1); \
  }
  switch(a->i)
  {
    case MpvToggle:
      mpv_send("cycle pause\n");
      break;
    case MpvLowerVolume:
      mpv_send("add volume -1\n");
      break;
    case MpvRaiseVolume:
      mpv_send("add volume +1\n");
      break;
    case MpvMuteVolume:
      mpv_send("cycle mute\n");
      break;
    case MpvNext:
      mpv_send("playlist_next\n");
      break;
    case MpvPrev:
      mpv_send("playlist_prev\n");
      break;
    case MpvSeekAhead10:
      mpv_send("seek +10\n");
      break;
    case MpvSeekAhead30:
      mpv_send("seek +30\n");
      break;
    case MpvSeekBehind10:
      mpv_send("seek -10\n");
      break;
    case MpvSeekBehind30:
      mpv_send("seek -30\n");
      break;
    case MpvQuit:
      mpv_send("quit\n");
      break;
    case MpvNextChapter:
      mpv_send("add chapter +1\n");
      break;
    case MpvPrevChapter:
      mpv_send("add chapter -1\n");
      break;
    case MpvSwitchAudioChannel:
      mpv_send("switch_audio\n");
      break;
  }
#undef mpv_send

  close(sfd);
}

void
tcl(Monitor * m)
{
	int x, y, h, w, mw, sw, bdw;
	unsigned int i, n;
	Client * c;

	for (n = 0, c = nexttiled(m->clients); c;
	        c = nexttiled(c->next), n++);

	if (n == 0)
		return;

	c = nexttiled(m->clients);

	mw = m->mfact * m->ww;
	sw = (m->ww - mw) / 2;
	bdw = (2 * c->bw);
	resize(c,
	       n < 3 ? m->wx : m->wx + sw,
	       m->wy,
	       n == 1 ? m->ww - bdw : mw - bdw,
	       m->wh - bdw,
	       False);

	if (--n == 0)
		return;

	w = (m->ww - mw) / ((n > 1) + 1);
	c = nexttiled(c->next);

	if (n > 1)
	{
		x = m->wx + ((n > 1) ? mw + sw : mw);
		y = m->wy;
		h = m->wh / (n / 2);

		if (h < bh)
			h = m->wh;

		for (i = 0; c && i < n / 2; c = nexttiled(c->next), i++)
		{
			resize(c,
			       x,
			       y,
			       w - bdw,
			       (i + 1 == n / 2) ? m->wy + m->wh - y - bdw : h - bdw,
			       False);

			if (h != m->wh)
				y = c->y + HEIGHT(c);
		}
	}

	x = (n + 1 / 2) == 1 ? mw : m->wx;
	y = m->wy;
	h = m->wh / ((n + 1) / 2);

	if (h < bh)
		h = m->wh;

	for (i = 0; c; c = nexttiled(c->next), i++)
	{
		resize(c,
		       x,
		       y,
		       (i + 1 == (n + 1) / 2) ? w - bdw : w - bdw,
		       (i + 1 == (n + 1) / 2) ? m->wy + m->wh - y - bdw : h - bdw,
		       False);

		if (h != m->wh)
			y = c->y + HEIGHT(c);
	}
}

int
main(int argc, char *argv[]) {
  if(argc == 2 && !strcmp("-v", argv[1]))
    die("dwm〜"VERSION"\n"
        "Built by 2ion at <https://github.com/2ion/dwm>\n"
        "Based on dwm <http://dwm.suckless.org>\n");
  else if(argc != 1)
    die("usage: dwm [-v]\n");
  if(!setlocale(LC_CTYPE, "") || !XSupportsLocale())
    fputs("warning: no locale support\n", stderr);
  if(!(dpy = XOpenDisplay(NULL)))
    die("dwm: cannot open display\n");
  checkotherwm();
  setup();
  scan();
  run();
  cleanup();
  XCloseDisplay(dpy);

  return EXIT_SUCCESS;
}
