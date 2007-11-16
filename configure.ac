AC_INIT(lightmediascanner, 0.1.0.1, gustavo.barbieri@openbossa.org)
AC_PREREQ(2.52)
AC_CONFIG_SRCDIR(configure.ac)
AC_CANONICAL_BUILD
AC_CANONICAL_HOST
AC_ISC_POSIX

AM_INIT_AUTOMAKE(1.6 dist-bzip2)
AM_CONFIG_HEADER(config.h)

AC_PROG_CC
AM_PROG_CC_STDC
AM_PROG_CC_C_O
AC_HEADER_STDC
AC_C_CONST

define([AC_LIBTOOL_LANG_CXX_CONFIG], [:])dnl
define([AC_LIBTOOL_LANG_F77_CONFIG], [:])dnl
AC_PROG_LIBTOOL

VMAJ=`echo $PACKAGE_VERSION | awk -F. '{printf("%s", $1);}'`
VMIN=`echo $PACKAGE_VERSION | awk -F. '{printf("%s", $2);}'`
VMIC=`echo $PACKAGE_VERSION | awk -F. '{printf("%s", $3);}'`
SNAP=`echo $PACKAGE_VERSION | awk -F. '{printf("%s", $4);}'`
version_info=`expr $VMAJ + $VMIN`":$VMIC:$VMIN"
AC_SUBST(version_info)

pluginsdir="${libdir}/lightmediascanner/plugins"

AC_SUBST(pluginsdir)
AS_AC_EXPAND(PLUGINSDIR, $pluginsdir)
AC_DEFINE_UNQUOTED(PLUGINSDIR, ["$PLUGINSDIR"], [Where plugins are installed.])

AC_CHECK_FUNCS(realpath)

AC_OUTPUT([
Makefile
src/Makefile
src/bin/Makefile
src/lib/Makefile
src/plugins/Makefile
src/plugins/dummy/Makefile
])
