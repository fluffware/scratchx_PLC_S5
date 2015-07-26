AC_INIT([PLCS2_helper], [0.1])
AC_CONFIG_SRCDIR([src/main.c])
AM_INIT_AUTOMAKE
AC_CONFIG_HEADERS([config.h])
AC_PROG_CC
AC_PROG_CXX
AC_SYS_LARGEFILE
AM_PATH_GLIB_2_0(2.30.0,,, [gobject gio gio-unix])

if true; then
# Use included libwebsocket
WEBSOCKETS_CFLAGS="-I `pwd`/libwebsockets/lib -I`pwd`/libwebsockets"
AC_SUBST(WEBSOCKETS_CFLAGS)
WEBSOCKETS_LIBS="-L `pwd`/libwebsockets/lib/ -l:libwebsockets.a -lz -lssl -lcrypto"
AC_SUBST(WEBSOCKETS_LIBS)
else
# Check for libwebsockets
AC_MSG_CHECKING([for libwebsockets]) 
if pkg-config libwebsockets --exists; then
WEBSOCKETS_CFLAGS=`pkg-config libwebsockets --cflags`
AC_SUBST(WEBSOCKETS_CFLAGS)
WEBSOCKETS_LIBS=`pkg-config libwebsockets --libs`
AC_SUBST(WEBSOCKETS_LIBS)
WEBSOCKETS_VERSION=`pkg-config libwebsockets --modversion`;
AC_MSG_RESULT(${WEBSOCKETS_VERSION});
else
AC_MSG_RESULT([not found])
fi
fi

# Check for JSON-GLib
AC_MSG_CHECKING([for JSON-GLib]) 
if pkg-config json-glib-1.0 --exists; then
JSONGLIB_CFLAGS=`pkg-config json-glib-1.0 --cflags`
AC_SUBST(JSONGLIB_CFLAGS)
JSONGLIB_LIBS=`pkg-config json-glib-1.0 --libs`
AC_SUBST(JSONGLIB_LIBS)
JSONGLIB_VERSION=`pkg-config json-glib-1.0 --modversion`;
AC_MSG_RESULT(${JSONGLIB_VERSION});
else
AC_MSG_RESULT([not found])
fi


AC_CONFIG_FILES([Makefile src/Makefile])
AC_OUTPUT
