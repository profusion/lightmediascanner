MAINTAINERCLEANFILES = Makefile.in

AM_CPPFLAGS = -I$(top_srcdir)/src/lib -I$(top_srcdir)/src/plugins/ogg @VORBIS_CFLAGS@

pkgdir = $(pluginsdir)
pkg_LTLIBRARIES = ogg.la
ogg_la_SOURCES = ogg.c
ogg_la_DEPENDENCIES = $(top_builddir)/config.h
ogg_la_LIBADD = $(top_builddir)/src/lib/liblightmediascanner.la @VORBIS_LIBS@
ogg_la_LDFLAGS = -module -avoid-version
