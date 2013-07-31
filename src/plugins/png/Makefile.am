MAINTAINERCLEANFILES = Makefile.in

AM_CPPFLAGS = -I$(top_srcdir)/src/lib -I$(top_srcdir)/src/plugins/png \
	@SQLITE3_CFLAGS@

pkgdir = $(pluginsdir)
pkg_LTLIBRARIES = png.la
png_la_SOURCES = png.c
png_la_DEPENDENCIES = $(top_builddir)/config.h
png_la_LIBADD = $(top_builddir)/src/lib/liblightmediascanner.la
png_la_LDFLAGS = -module -avoid-version -shared
