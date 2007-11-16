MAINTAINERCLEANFILES = Makefile.in

AM_CPPFLAGS = -I$(top_srcdir)/src/lib

include_HEADERS = lightmediascanner.h lightmediascanner_plugin.h

lib_LTLIBRARIES = liblightmediascanner.la

liblightmediascanner_la_SOURCES = lightmediascanner.c
liblightmediascanner_la_LIBADD = -ldl
liblightmediascanner_la_LDFLAGS = -version-info @version_info@
