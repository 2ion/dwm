// dwm - config.h

#define CPU_LOWER "825Mhz"
#define CPU_UPPER "1.65Ghz"
#define MODKEY Mod4Mask
#define TAGKEYS(KEY,TAG) \
  { MODKEY,                       KEY,      view,           {.ui = 1 << TAG} }, \
  { MODKEY|ControlMask,           KEY,      toggleview,     {.ui = 1 << TAG} }, \
  { MODKEY|ShiftMask,             KEY,      tag,            {.ui = 1 << TAG} }, \
  { MODKEY|ControlMask|ShiftMask, KEY,      toggletag,      {.ui = 1 << TAG} },
#define ACTION(action) DCMD("/home/joj/.actions.d/" action)
#define DCMD(cmd) { cmd, "-b", \
    "-fn", dmenufont, \
    "-nb", normbgcolor, \
    "-nf", normfgcolor, \
    "-sb", selbgcolor, \
    "-sf", selfgcolor, \
    NULL }

static const char font[]            = "Noto Sans 10";
static const char dmenufont[]       = "-*-tamsyn-medium-r-*-*-20-*-*-*-*-*-*-*";
static const char normbordercolor[] = "#281920";
static const char selbordercolor[]  = "#5f0916";
static const char normbgcolor[]     = "#343c45";
static const char normfgcolor[]     = "#ededed";
static const char selbgcolor[]      = "#1c2126";
static const char selfgcolor[]      = "#ededed";
static const unsigned int borderpx  = 1;        
static const unsigned int snap      = 5;       
static const Bool showbar           = False;     
static const Bool topbar            = True; 
enum {
  TagTerm   = 0,
  TagWeb    = 1,
  TagDic    = 2,
  TagPdf    = 3,
  TagMisc   = 4,
  TagStash  = 5,
  TagIota   = 6,
  TagKappa  = 7,
  TagLambda = 8};
static const char *tags[]           = { "term", "web", "dic", "pdf", "misc", "stash", "ι", "κ", "λ" };
static const float mfact            = 0.62; 
static const int nmaster            = 1;   
static const int cfg_mpdcmd_retries = 4;
static const int cfg_mpdcmd_notify_enable = 1;
static const int cfg_mpdcmd_notify_retries = 2;
static const int cfg_mpdcmd_notify_timeout = 5;
static const char cfg_mpdcmd_mpdhost[] = "/home/joj/.mpd_socket";
static unsigned cfg_mpdcmd_mpdport = 6600;
static int voldelta                 = 4;
static const Bool resizehints       = False;
static const Layout layouts[]       = {
    { "[+]", monocle   },
    { ">>=", tile      },    
    { "TTT", bstack    },
    { "><>", NULL      },
    { ">>-", deck      }};

#include "rules.h"

static const char *cmd_backlightup[]	    = { "xbacklight", "+10", NULL };
static const char *cmd_backlightdown[]	    = { "xbacklight", "-10", NULL };
static const char *cmd_terminal[]           = { "urxvt", NULL };
static const char *cmd_browser[]            = { "firefox", NULL };
static const char *cmd_lock[]               = { "xscreensaver-command", "-lock", NULL };
static const char *cmd_gjiten[]             = { "gjiten", "-v", NULL };
static const char *cmd_xkill[]              = { "xkill", NULL };
static const char *cmd_fetchmail[]          = { "fetchmail", NULL };
static const char *cmd_cpu_lower[]          = { "sudo", "cpufreq-set", "-u", CPU_LOWER , NULL };
static const char *cmd_cpu_upper[]          = { "sudo", "cpufreq-set", "-u", CPU_UPPER , NULL };
static const char *cmd_action[]             = ACTION("runaction");
static const char *cmd_tmux[]               = ACTION("tmux");
static const char *cmd_backlight[]          = ACTION("backlight-off");
static const char *cmd_dmenu[]              = DCMD("dmenu_run");
static const char *cmd_sleep[]              = ACTION("sleep");
static const char *cmd_webcam[]             = { "/usr/bin/mplayer", "tv:///", NULL };
static const char *cmd_skippy[]             = { "skippy-xd", NULL };
static const char *cmd_mozc_config[]        = { "/usr/lib/mozc/mozc_tool", "--mode=config_dialog", NULL };
static const char *cmd_mozc_register[]      = { "/usr/lib/mozc/mozc_tool", "--mode=word_register_dialog", NULL };
static const char *cmd_mozc_pad[]           = { "/usr/lib/mozc/mozc_tool", "--mode=hand_writing", NULL };

static Key keys[] = {
    /* Alpha keys */
  { MODKEY,                       XK_a,      spawn,          {.v = cmd_action}},
  { MODKEY,                       XK_p,      spawn,          {.v = cmd_dmenu}},
  { MODKEY|ShiftMask,             XK_p,      spawn,          {.v = cmd_tmux}},
  { MODKEY|ShiftMask,             XK_Return, spawn,          {.v = cmd_terminal}},
  { MODKEY|ControlMask,           XK_Return, spawn,          {.v = cmd_browser}},
  { MODKEY,                       XK_x,      spawn,          {.v = cmd_xkill}},

  { MODKEY,                       XK_b,      togglebar,      {0}},
  { MODKEY,                       XK_j,      focusstack,     {.i = +1 }},
  { MODKEY,                       XK_k,      focusstack,     {.i = -1 }},
  { MODKEY|ControlMask,           XK_j,      pushdown,       {0}},
  { MODKEY|ControlMask,           XK_k,      pushup,         {0}},
  { MODKEY,                       XK_i,      incnmaster,     {.i = +1 }},
  { MODKEY,                       XK_d,      incnmaster,     {.i = -1 }},
  { MODKEY,                       XK_h,      setmfact,       {.f = -0.05}},
  { MODKEY,                       XK_l,      setmfact,       {.f = +0.05}},
  { MODKEY,                       XK_Return, zoom,           {0}},
  { MODKEY,                       XK_Tab,    view,           {0}},
  { MODKEY|ShiftMask,             XK_c,      killclient,     {0}},
  { MODKEY,                       XK_t,      setlayout,      {.v = &layouts[1]}}, // tiled
  { MODKEY|ShiftMask,             XK_t,      setlayout,      {.v = &layouts[4]}}, // deck
  { MODKEY,                       XK_s,      setlayout,      {.v = &layouts[2]}}, // bstack
  { MODKEY,                       XK_f,      setlayout,      {.v = &layouts[3]}}, // float
  { MODKEY,                       XK_m,      setlayout,      {.v = &layouts[0]}}, // mononocle
  { MODKEY,                       XK_space,  setlayout,      {0}},
  { MODKEY|ShiftMask,             XK_space,  togglefloating, {0}},
  { MODKEY,                       XK_0,      view,            {.ui = ~0 }},
  { MODKEY|ShiftMask,             XK_0,      tag,             {.ui = ~0 }},
  { MODKEY,                       XK_minus,  focusmon,        {.i = -1 }},
  { MODKEY,                       XK_plus,   focusmon,        {.i = +1 }},
  { MODKEY|ShiftMask,             XK_minus,  tagmon,          {.i = -1 }},
  { MODKEY|ShiftMask,             XK_plus,   tagmon,          {.i = +1 }},
  { MODKEY,                       XK_comma,  cycle,           {.i = -1 }},
  { MODKEY,                       XK_period, cycle,           {.i = +1 }},
  { MODKEY|ShiftMask,             XK_comma,  tagcycle,        {.i = -1 }},
  { MODKEY|ShiftMask,             XK_period, tagcycle,        {.i = +1 }},
  { MODKEY,                       XK_End,    spawn,           {.v = cmd_lock }},
  { MODKEY,                       XK_BackSpace, spawn,        {.v = cmd_skippy }},
  { MODKEY,                       XK_o,      changeopacity,   {.f = +0.05 }},
  { MODKEY|ShiftMask,             XK_o,      changeopacity,   {.f = -0.05 }},
  { MODKEY|ControlMask,           XK_o,      setopacity,      {.f = 1.0 }},

  /* XF86 KEYS */

  { False,                        XF86XK_AudioLowerVolume,    mpdcmd, { .i = MpdLowerVolume }},
  { False,                        XF86XK_AudioRaiseVolume,    mpdcmd, { .i = MpdRaiseVolume }},
  { False,                        XF86XK_AudioMute,           mpdcmd, { .i = MpdMuteVolume }},
  { False,                        XF86XK_LaunchA,             mpdcmd, { .i = MpdTogglePause }},
  { False,                        XF86XK_Search,              mpdcmd, { .i = MpdPrev }},
  { False,                        XF86XK_Explorer,            mpdcmd, { .i = MpdNext }},
  { False,                        XF86XK_MonBrightnessDown,   spawn, {.v = cmd_backlightdown }},
  { False,                        XF86XK_MonBrightnessUp,     spawn, {.v = cmd_backlightup }},
  { MODKEY,                       XK_Delete,                  mpdcmd, { .i = MpdPlayAgain }},
  { MODKEY,                       XK_c,                       mpdcmd_savepos, { .i = 0 }},
  { MODKEY,                       XK_v,                       mpdcmd_loadpos, { .i = 0 }},
  { MODKEY|ShiftMask,             XK_c,                       mpdcmd_savepos, { .i = 1 }},
  { MODKEY|ShiftMask,             XK_v,                       mpdcmd_loadpos, { .i = 1 }},
  { MODKEY|ControlMask,           XK_c,                       mpdcmd_savepos, { .i = 2 }},
  { MODKEY|ControlMask,           XK_v,                       mpdcmd_loadpos, { .i = 2 }},
  { False,                        XF86XK_Sleep,               spawn,  { .v = cmd_sleep }},
  { False,                        XF86XK_Display,             spawn,  { .v = cmd_backlight }},
  { False,                        XF86XK_WebCam,              spawn,  { .v = cmd_webcam }},

  /* F ROW */

  { MODKEY,                       XK_F5,     spawn,          {.v = cmd_gjiten }},
  { MODKEY,                       XK_F6,     spawn,          {.v = cmd_cpu_lower }},
  { MODKEY,                       XK_F7,     spawn,          {.v = cmd_cpu_upper }},
  { MODKEY,                       XK_F9,     spawn,          {.v = cmd_fetchmail}},
  { MODKEY,                       XK_F10,    spawn,          {.v = cmd_mozc_config}},
  { MODKEY,                       XK_F11,    spawn,          {.v = cmd_mozc_register}},
  { MODKEY,                       XK_F12,    spawn,          {.v = cmd_mozc_pad}},

  /* NUMBER ROW */

  TAGKEYS(                        XK_1,                      0)
  TAGKEYS(                        XK_2,                      1)
  TAGKEYS(                        XK_3,                      2)
  TAGKEYS(                        XK_4,                      3)
  TAGKEYS(                        XK_5,                      4)
  TAGKEYS(                        XK_6,                      5)
  TAGKEYS(                        XK_7,                      6)
  TAGKEYS(                        XK_8,                      7)
  TAGKEYS(                        XK_9,                      8)
  { MODKEY|ShiftMask,             XK_q,      quit,           {0}},
};

/* click can be ClkLtSymbol, ClkStatusText, ClkWinTitle, ClkClientWin, or ClkRootWin */
static Button buttons[] = {
  /* click                event mask      button          function        argument */
  { ClkLtSymbol,          0,              Button1,        setlayout,      {0}},
  { ClkLtSymbol,          0,              Button3,        setlayout,      {.v = &layouts[2]}},
  { ClkWinTitle,          0,              Button2,        zoom,           {0}},
  { ClkClientWin,         MODKEY,         Button1,        movemouse,      {0}},
  { ClkClientWin,         MODKEY,         Button2,        togglefloating, {0}},
  { ClkClientWin,         MODKEY,         Button3,        resizemouse,    {0}},
  { ClkTagBar,            0,              Button1,        view,           {0}},
  { ClkTagBar,            0,              Button3,        toggleview,     {0}},
  { ClkTagBar,            MODKEY,         Button1,        tag,            {0}},
  { ClkTagBar,            MODKEY,         Button3,        toggletag,      {0}},
};

