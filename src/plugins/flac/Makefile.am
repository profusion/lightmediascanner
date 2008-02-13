MAINTAINERCLEANFILES = Makefile.in

AM_CPPFLAGS = -I$(top_srcdir)/src/lib -I$(top_srcdir)/src/plugins/flac @FLAC_CFLAGS@

pkgdir = $(pluginsdir)
pkg_LTLIBRARIES = flac.la
flac_la_SOURCES = flac.c
flac_la_DEPENDENCIES = $(top_builddir)/config.h
flac_la_LIBADD = $(top_builddir)/src/lib/liblightmediascanner.la @FLAC_LIBS@
flac_la_LDFLAGS = -module -avoid-version
