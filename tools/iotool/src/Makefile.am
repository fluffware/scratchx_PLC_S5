appdir=@datadir@/applications
icondir=@datadir@/icons
themedir=$(icondir)/hicolor
app48x48dir=$(themedir)/48x48/apps
app32x32dir=$(themedir)/32x32/apps
app22x22dir=$(themedir)/22x22/apps

AM_CPPFLAGS= -I @builddir@ -I. @GTK_CFLAGS@ @GLIB_CFLAGS@

AM_CFLAGS="-std=c99"

bin_PROGRAMS = iotool

iotool_SOURCES = iotool.c \
serial.c serial.h \
PLC_comm.c PLC_comm.h	

iotool_LDADD = @GTK_LIBS@ @GLIB_LIBS@-lm

version_info.h: FORCE
	echo "#define PKG_VERSION \"$(VERSION)\"" > $@
	echo "#define GIT_COMMIT \"`git rev-parse HEAD`\"" >> $@


.PHONY: FORCE

gsettings_SCHEMAS =

@GSETTINGS_RULES@

BUILT_SOURCES= version_info.h

CLEANFILES = version_info.h

EXTRA_DIST=
