MAINTAINERCLEANFILES = Makefile.in

AM_CPPFLAGS = -I$(top_srcdir)/src/lib -I$(top_srcdir)/src/plugins/asf \
	@SQLITE3_CFLAGS@

pkgdir = $(pluginsdir)
pkg_LTLIBRARIES = asf.la
asf_la_SOURCES = asf.c
asf_la_DEPENDENCIES = $(top_builddir)/config.h
asf_la_LIBADD = $(top_builddir)/src/lib/liblightmediascanner.la
asf_la_LDFLAGS = -module -avoid-version
