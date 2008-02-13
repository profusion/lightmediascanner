MAINTAINERCLEANFILES = Makefile.in

AM_CPPFLAGS = -I$(top_srcdir)/src/lib -I$(top_srcdir)/src/plugins/id3

pkgdir = $(pluginsdir)
pkg_LTLIBRARIES = id3.la
id3_la_SOURCES = id3.c
id3_la_DEPENDENCIES = $(top_builddir)/config.h
id3_la_LIBADD = $(top_builddir)/src/lib/liblightmediascanner.la
id3_la_LDFLAGS = -module -avoid-version
