MAINTAINERCLEANFILES = Makefile.in

AM_CPPFLAGS = -I$(top_srcdir)/src/lib -I$(top_srcdir)/src/plugins/id3lib @ID3LIB_CFLAGS@

pkgdir = $(pluginsdir)
pkg_LTLIBRARIES = id3lib.la
id3lib_la_SOURCES = id3lib.cpp
id3lib_la_DEPENDENCIES = $(top_builddir)/config.h
id3lib_la_LIBADD = $(top_builddir)/src/lib/liblightmediascanner.la @ID3LIB_LIBS@
id3lib_la_LDFLAGS = -module -avoid-version
