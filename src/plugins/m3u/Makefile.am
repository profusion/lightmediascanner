MAINTAINERCLEANFILES = Makefile.in

AM_CPPFLAGS = -I$(top_srcdir)/src/lib -I$(top_srcdir)/src/plugins/m3u

pkgdir = $(pluginsdir)
pkg_LTLIBRARIES = m3u.la
m3u_la_SOURCES = m3u.c
m3u_la_DEPENDENCIES = $(top_builddir)/config.h
m3u_la_LIBADD = $(top_builddir)/src/lib/liblightmediascanner.la
m3u_la_LDFLAGS = -module -avoid-version
