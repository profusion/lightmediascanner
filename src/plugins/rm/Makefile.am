MAINTAINERCLEANFILES = Makefile.in

AM_CPPFLAGS = -I$(top_srcdir)/src/lib -I$(top_srcdir)/src/plugins/rm \
	@SQLITE3_CFLAGS@

pkgdir = $(pluginsdir)
pkg_LTLIBRARIES = rm.la
rm_la_SOURCES = rm.c
rm_la_DEPENDENCIES = $(top_builddir)/config.h
rm_la_LIBADD = $(top_builddir)/src/lib/liblightmediascanner.la
rm_la_LDFLAGS = -module -avoid-version -shared
