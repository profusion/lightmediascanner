MAINTAINERCLEANFILES = Makefile.in

AM_CPPFLAGS = -I$(top_srcdir)/src/lib -I$(top_srcdir)/src/plugins/mp4 \
	@SQLITE3_CFLAGS@

pkgdir = $(pluginsdir)
pkg_LTLIBRARIES = mp4.la
mp4_la_SOURCES = mp4.c
mp4_la_DEPENDENCIES = $(top_builddir)/config.h
mp4_la_LIBADD = $(top_builddir)/src/lib/liblightmediascanner.la @MP4V2_LIBS@
mp4_la_LDFLAGS = -module -avoid-version -shared
mp4_la_CFLAGS = -fvisibility=hidden
