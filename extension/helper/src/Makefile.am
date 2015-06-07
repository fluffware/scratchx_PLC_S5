SUBDIRS=

AM_CFLAGS = @GLIB_CFLAGS@ @MICROHTTPD_CFLAGS@ @JSONGLIB_CFLAGS@ -Wno-long-long
AM_CXXFLAGS = @GLIB_CFLAGS@ -Wno-long-long

noinst_PROGRAMS = 
bin_PROGRAMS = PLCS5_helper PLCS5_tool

PLCS5_helper_SOURCES = main.c \
httpd.c httpd.h \
httpd_marshal.c httpd_marshal.h


PLCS5_helper_LDADD= @GLIB_LIBS@ @MICROHTTPD_LIBS@ @JSONGLIB_LIBS@ -lm

PLCS5_tool_SOURCES = s5_tool.c \
serial.c serial.h \
AS511_comm.c AS511_comm.h


PLCS5_tool_LDADD= @GLIB_LIBS@ -lm

%marshal.h: %marshal.list
	glib-genmarshal --header --prefix="$*marshal" $< > $@.tmp && \
        mv $@.tmp $@

%marshal.c: %marshal.list
	glib-genmarshal --body --prefix="$*marshal" $< > $@.tmp && \
        mv $@.tmp $@

BUILT_SOURCES =  httpd_marshal.c httpd_marshal.h
