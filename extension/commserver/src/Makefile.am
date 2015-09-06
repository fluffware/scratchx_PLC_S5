SUBDIRS=

AM_CFLAGS = @GLIB_CFLAGS@ @WEBSOCKETS_CFLAGS@ @JSONGLIB_CFLAGS@ -Wno-long-long -std=c99 -I ../../../include
AM_CXXFLAGS = @GLIB_CFLAGS@

noinst_PROGRAMS = 
bin_PROGRAMS = PLC_S5_commserver

PLC_S5_commserver_SOURCES = main.c \
websocket_server.c websocket_server.h \
PLC_IO_server.c PLC_IO_server.h \
PLC_IO_comm.c PLC_IO_comm.h \
serial.c serial.h


PLC_S5_commserver_LDADD= @GLIB_LIBS@ @WEBSOCKETS_LIBS@ @JSONGLIB_LIBS@ -lm

%marshal.h: %marshal.list
	glib-genmarshal --header --prefix="$*marshal" $< > $@.tmp && \
        mv $@.tmp $@

%marshal.c: %marshal.list
	glib-genmarshal --body --prefix="$*marshal" $< > $@.tmp && \
        mv $@.tmp $@

BUILT_SOURCES =  #httpd_marshal.c httpd_marshal.h
