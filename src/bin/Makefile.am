MAINTAINERCLEANFILES = Makefile.in

AM_CPPFLAGS = \
	-I$(top_srcdir)/src/lib

noinst_PROGRAMS = \
	test


test_SOURCES = test.c
test_LDADD = $(top_builddir)/src/lib/liblightmediascanner.la @SQLITE3_LIBS@
test_DEPENDENCIES = $(top_builddir)/src/lib/liblightmediascanner.la
