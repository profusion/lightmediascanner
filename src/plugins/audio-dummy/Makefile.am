MAINTAINERCLEANFILES = Makefile.in

AM_CPPFLAGS = -I$(top_srcdir)/src/lib -I$(top_srcdir)/src/plugins/audio-dummy

pkgdir = $(pluginsdir)
pkg_LTLIBRARIES = audio-dummy.la
audio_dummy_la_SOURCES = audio-dummy.c
audio_dummy_la_DEPENDENCIES = $(top_builddir)/config.h
audio_dummy_la_LIBADD = $(top_builddir)/src/lib/liblightmediascanner.la
audio_dummy_la_LDFLAGS = -module -avoid-version
