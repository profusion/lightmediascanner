MAINTAINERCLEANFILES = Makefile.in

AM_CPPFLAGS = -I$(top_srcdir)/src/lib -I$(top_srcdir)/src/plugins/ogg \
	@VORBIS_CFLAGS@ @SQLITE3_CFLAGS@

pkgdir = $(pluginsdir)
pkg_LTLIBRARIES = ogg.la
ogg_la_SOURCES = ogg.c

if USE_TREMOR
ogg_la_SOURCES += lms_ogg_tremor.c
else
ogg_la_SOURCES += lms_ogg_vorbis.c
endif

ogg_la_DEPENDENCIES = $(top_builddir)/config.h
ogg_la_LIBADD = $(top_builddir)/src/lib/liblightmediascanner.la @VORBIS_LIBS@
ogg_la_LDFLAGS = -module -avoid-version -shared

noinst_HEADERS= lms_ogg_private.h
ogg_la_CFLAGS = -fvisibility=hidden
