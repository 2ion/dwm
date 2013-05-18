# dwm version
VERSION = 6.0

# Customize below to fit your system

# paths
PREFIX = /usr/local
MANPREFIX = ${PREFIX}/share/man

X11INC = /usr/X11R6/include
X11LIB = /usr/X11R6/lib

# Xinerama
XINERAMALIBS = -L${X11LIB} -lXinerama
XINERAMAFLAGS = -DXINERAMA

# LUA
LUAINC = `pkg-config --cflags lua5.2`
LUALIB = `pkg-config --libs lua5.2`

# includes and libs
INCS = -I. -I/usr/include -I${X11INC} `pkg-config --cflags xft pango pangoxft`
LIBS = -L/usr/lib -lc -L${X11LIB} -lX11 ${XINERAMALIBS} `pkg-config --libs xft pango pangoxft libmpdclient`

# flags
CPPFLAGS = -DVERSION=\"${VERSION}\" ${XINERAMAFLAGS}
#CFLAGS = -g -std=c99 -pedantic -Wall -O0 ${INCS} ${CPPFLAGS}
CFLAGS = -std=c99 -pedantic -Wall -O2 ${INCS} ${CPPFLAGS}
#LDFLAGS = -g ${LIBS}
LDFLAGS = -s ${LIBS}

# Solaris
#CFLAGS = -fast ${INCS} -DVERSION=\"${VERSION}\"
#LDFLAGS = ${LIBS}

# compiler and linker
CC = cc
