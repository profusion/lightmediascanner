MAINTAINERCLEANFILES = Makefile.in

AM_CPPFLAGS = \
	-I$(top_srcdir)/src/lib \
	@SQLITE3_CFLAGS@

noinst_PROGRAMS = \
	test \
	list-parsers


test_SOURCES = test.c
test_LDADD = $(top_builddir)/src/lib/liblightmediascanner.la @SQLITE3_LIBS@
test_DEPENDENCIES = $(top_builddir)/src/lib/liblightmediascanner.la


list_parsers_SOURCES = list-parsers.c
list_parsers_LDADD = $(top_builddir)/src/lib/liblightmediascanner.la \
	@SQLITE3_LIBS@
list_parsers_DEPENDENCIES = $(top_builddir)/src/lib/liblightmediascanner.la
