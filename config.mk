VERSION = 6.24

PREFIX = /usr/local
MANPREFIX = ${PREFIX}/share/man

X11INC = /usr/X11R6/include
X11LIB = /usr/X11R6/lib

XINERAMALIBS = -L${X11LIB} -lXinerama
XINERAMAFLAGS = -DXINERAMA

INCS = -I. -I/usr/include -I${X11INC} `pkg-config --cflags xft pango pangoxft libnotify`
LIBS = -L/usr/lib -lc -lrt -L${X11LIB} -lX11 ${XINERAMALIBS} `pkg-config --libs xft pango pangoxft libmpdclient libnotify`

CPPFLAGS = -DVERSION=\"${VERSION}\" ${XINERAMAFLAGS} -D_FORTIFY_SOURCE=2 -DDEBUG
CFLAGS = -std=gnu99 -Wall -march=native -pipe -fstack-protector-strong --param=ssp-buffer-size=4 -O2 ${INCS} ${CPPFLAGS}
LDFLAGS = -s ${LIBS}

CC = cc
