MAINTAINERCLEANFILES = Makefile.in

AM_CPPFLAGS = -I$(top_srcdir)/src/lib -I$(top_srcdir)/src/plugins/video-dummy \
	@SQLITE3_CFLAGS@

pkgdir = $(pluginsdir)
pkg_LTLIBRARIES = video-dummy.la
video_dummy_la_SOURCES = video-dummy.c
video_dummy_la_DEPENDENCIES = $(top_builddir)/config.h
video_dummy_la_LIBADD = $(top_builddir)/src/lib/liblightmediascanner.la
video_dummy_la_LDFLAGS = -module -avoid-version
