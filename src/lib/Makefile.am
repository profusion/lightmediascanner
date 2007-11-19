MAINTAINERCLEANFILES = Makefile.in

AM_CPPFLAGS = -I$(top_srcdir)/src/lib

include_HEADERS = \
	lightmediascanner.h \
	lightmediascanner_plugin.h \
	lightmediascanner_utils.h

lib_LTLIBRARIES = liblightmediascanner.la

liblightmediascanner_la_SOURCES = \
	lightmediascanner.c \
	lightmediascanner_utils.c
liblightmediascanner_la_LIBADD = -ldl @SQLITE3_LIBS@
liblightmediascanner_la_LDFLAGS = -version-info @version_info@
