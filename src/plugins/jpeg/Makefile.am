MAINTAINERCLEANFILES = Makefile.in

AM_CPPFLAGS = -I$(top_srcdir)/src/lib -I$(top_srcdir)/src/plugins/jpeg \
	@SQLITE3_CFLAGS@

pkgdir = $(pluginsdir)
pkg_LTLIBRARIES = jpeg.la
jpeg_la_SOURCES = jpeg.c
jpeg_la_DEPENDENCIES = $(top_builddir)/config.h
jpeg_la_LIBADD = $(top_builddir)/src/lib/liblightmediascanner.la
jpeg_la_LDFLAGS = -module -avoid-version
