MAINTAINERCLEANFILES = Makefile.in

AM_CPPFLAGS = -I$(top_srcdir)/src/lib -I$(top_srcdir)/src/plugins/dummy

pkgdir = $(pluginsdir)
pkg_LTLIBRARIES = dummy.la
dummy_la_SOURCES = dummy.c
dummy_la_DEPENDENCIES = $(top_builddir)/config.h
dummy_la_LIBADD = $(top_builddir)/src/lib/liblightmediascanner.la
dummy_la_LDFLAGS = -module -avoid-version
