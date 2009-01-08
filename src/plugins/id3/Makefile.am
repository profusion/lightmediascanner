MAINTAINERCLEANFILES = Makefile.in

AM_CPPFLAGS = -I$(top_srcdir)/src/lib -I$(top_srcdir)/src/plugins/id3 \
	@SQLITE3_CFLAGS@

pkgdir = $(pluginsdir)
pkg_LTLIBRARIES = id3.la
id3_la_SOURCES = id3.c id3v1_genres.c
id3_la_DEPENDENCIES = $(top_builddir)/config.h id3v1_genres.def
id3_la_LIBADD = $(top_builddir)/src/lib/liblightmediascanner.la
id3_la_LDFLAGS = -module -avoid-version

id3v1_genres.c: $(srcdir)/id3v1_genres.def $(srcdir)/id3v1_genres_gen.awk
	$(AWK) -f $(srcdir)/id3v1_genres_gen.awk $(srcdir)/id3v1_genres.def > $@

EXTRA_DIST = id3v1_genres.def id3v1_genres_gen.awk
BUILT_SOURCES = id3v1_genres.c
CLEANFILES = $(BUILT_SOURCES)
