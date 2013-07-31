MAINTAINERCLEANFILES = Makefile.in

AM_CPPFLAGS = -I$(top_srcdir)/src/lib -I$(top_srcdir)/src/plugins/pls \
	@SQLITE3_CFLAGS@

pkgdir = $(pluginsdir)
pkg_LTLIBRARIES = pls.la
pls_la_SOURCES = pls.c
pls_la_DEPENDENCIES = $(top_builddir)/config.h
pls_la_LIBADD = $(top_builddir)/src/lib/liblightmediascanner.la
pls_la_LDFLAGS = -module -avoid-version -shared
