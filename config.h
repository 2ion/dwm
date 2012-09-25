/*
 * dwm window manager configuration
 * */

#include "push.c"
#include "bstack.c"

#define MODKEY Mod4Mask
#define TAGKEYS(KEY,TAG) \
	{ MODKEY,                       KEY,      view,           {.ui = 1 << TAG} }, \
	{ MODKEY|ControlMask,           KEY,      toggleview,     {.ui = 1 << TAG} }, \
	{ MODKEY|ShiftMask,             KEY,      tag,            {.ui = 1 << TAG} }, \
	{ MODKEY|ControlMask|ShiftMask, KEY,      toggletag,      {.ui = 1 << TAG} },
#define DCMD(cmd) { cmd, \
    "-fn", dmenufont, \
    "-nb", normbgcolor, \
    "-nf", normfgcolor, \
    "-sb", selbgcolor, \
    "-sf", selfgcolor, \
    NULL }

static const char font[]            = "Terminus,Adobe Heiti Std 7";
static const char dmenufont[]       = "-xos4-terminus-medium-r-normal-*-17-*-*-*-*-*-*-*";
static const char normbordercolor[] = "#171c12";
static const char normbgcolor[]     = "#101010";
static const char normfgcolor[]     = "#4f4f4f";
static const char selbordercolor[]  = "#687f50";
static const char selbgcolor[]      = "#101010";
static const char selfgcolor[]      = "#afd787";
static const unsigned int borderpx  = 1;        
static const unsigned int snap      = 10;       
static const Bool showbar           = False;     
static const Bool topbar            = False; 
static const char *tags[]           = { "一", "二", "三", "四", "五", "六", "七", "八", "九" };
static const float mfact            = 0.62; 
static const int nmaster            = 1;   
static const Bool resizehints       = False;
static const Layout layouts[]       = {
    { "[+]", monocle   },
    { "[]=", tile      },    
    { "TTT", bstack    },
    { "><>", NULL      }};
static const Rule rules[]           = {
	/* class                    instance    title       tags mask     isfloating   monitor */
	{ "Gimp",                   NULL,       NULL,       0,            True,        -1 },
	{ "Firefox",                NULL,       NULL,       1 << 1,       False,       -1 },
    { "Luakit",                 NULL,       NULL,       1 << 1,       False,       -1 },
    { "MPlayer",                NULL,       NULL,       0,            True,        -1 },
    { "mplayer2",               NULL,       NULL,       0,            True,        -1 },
    { "Xchat",                  NULL,       NULL,       1 << 6,       False,       -1 },
    { "Gjiten",                 NULL,       NULL,       1 << 2,       False,       -1 },
    { "URxvt",                  NULL,       "ichi:",    1 << 0,       False,       -1 }
};

static const char *cmd_terminal[]           = { "x-terminal-emulator", NULL };
static const char *cmd_browser[]            = { "x-www-browser", NULL };
static const char *cmd_volume_raise[]       = { "amixer", "-c", "0", "sset", "Master,0", "1dB+", NULL };
static const char *cmd_volume_lower[]       = { "amixer", "-c", "0", "sset", "Master,0", "1dB-", NULL };
static const char *cmd_volume_mute[]        = { "amixer", "-c", "0", "sset", "Master,0", "0",    NULL };
static const char *cmd_lock[]               = { "xscreensaver-command", "-lock", NULL };
static const char *cmd_gjiten[]             = { "gjiten", "-v", NULL };
static const char *cmd_xkill[]              = { "xkill", NULL };
static const char *cmd_fetchmail[]          = { "fetchmail", NULL };
static const char *cmd_cpu_lower[]          = { "cpufreq-set", "-u", "800Mhz", NULL };
static const char *cmd_cpu_upper[]          = { "cpufreq-set", "-u", "1.6Ghz", NULL };
static const char *cmd_action[]             = DCMD("/home/joj/.actions.d/runaction");
static const char *cmd_tmux[]               = DCMD("/home/joj/.actions.d/tmux");
static const char *cmd_dmenu[]              = DCMD("dmenu_run");

static Key keys[] = {
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
    { False,                       XF86XK_AudioLowerVolume, spawn, {.v = cmd_volume_lower }},
    { False,                       XF86XK_AudioRaiseVolume, spawn, {.v = cmd_volume_raise }},
    { False,                       XF86XK_AudioMute, spawn, {.v = cmd_volume_mute }},
    { MODKEY,                       XK_End,    spawn,           {.v = cmd_lock }},
    { MODKEY,                       XK_F1,     mpdcmd,          {.i = 1 }},
    { MODKEY,                       XK_F2,     mpdcmd,          {.i = 2 }},
    { MODKEY,                       XK_F3,     mpdcmd,          {.i = 3 }},
    { MODKEY,                       XK_F5,     spawn,          {.v = cmd_gjiten }},
    { MODKEY,                       XK_F6,     spawn,          {.v = cmd_cpu_lower }},
    { MODKEY,                       XK_F7,     spawn,          {.v = cmd_cpu_upper }},
    { MODKEY,                       XK_F9,     spawn,          {.v = cmd_fetchmail}},
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
	{ ClkStatusText,        0,              Button2,        spawn,          {.v = cmd_terminal }},
	{ ClkClientWin,         MODKEY,         Button1,        movemouse,      {0}},
	{ ClkClientWin,         MODKEY,         Button2,        togglefloating, {0}},
	{ ClkClientWin,         MODKEY,         Button3,        resizemouse,    {0}},
	{ ClkTagBar,            0,              Button1,        view,           {0}},
	{ ClkTagBar,            0,              Button3,        toggleview,     {0}},
	{ ClkTagBar,            MODKEY,         Button1,        tag,            {0}},
	{ ClkTagBar,            MODKEY,         Button3,        toggletag,      {0}},
};

