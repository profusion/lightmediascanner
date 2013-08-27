MAINTAINERCLEANFILES = \
	Makefile.in \
	aclocal.m4 \
	compile \
	config.guess \
	config.h.in \
	config.sub \
	configure \
	depcomp \
	install-sh \
	ltmain.sh \
	missing

SUBDIRS = src m4

EXTRA_DIST = \
	README \
	AUTHORS \
	COPYING \
	Doxyfile \
	lightmediascanner.spec

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = lightmediascanner.pc

ACLOCAL_AMFLAGS = -I m4

if BUILD_DAEMON
servicedir = @dbusservicedir@
service_DATA = org.lightmediascanner.service
endif
