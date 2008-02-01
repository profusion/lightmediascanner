MAINTAINERCLEANFILES = Makefile.in

AM_CPPFLAGS = -I$(top_srcdir)/src/lib -I$(top_srcdir)/src/plugins/aac

pkgdir = $(pluginsdir)
pkg_LTLIBRARIES = aac.la
aac_la_SOURCES = aac.c
aac_la_DEPENDENCIES = $(top_builddir)/config.h
aac_la_LIBADD = $(top_builddir)/src/lib/liblightmediascanner.la
aac_la_LDFLAGS = -module -avoid-version
