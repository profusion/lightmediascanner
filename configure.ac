AC_INIT(lightmediascanner, 0.1.0.1, gustavo.barbieri@openbossa.org)
AC_PREREQ(2.52)
AC_CONFIG_SRCDIR(configure.ac)
AC_CANONICAL_BUILD
AC_CANONICAL_HOST
AC_ISC_POSIX

AM_INIT_AUTOMAKE(1.6 dist-bzip2)
AM_CONFIG_HEADER(config.h)
AC_SUBST(ACLOCAL_AMFLAGS, "-I m4")
_XTERM_COLORS

AC_PROG_CC
AM_PROG_CC_STDC
AM_PROG_CC_C_O
AC_HEADER_STDC
AC_C_CONST

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

# required modules
PKG_CHECK_MODULES(SQLITE3, [sqlite3 >= 3.3])

# plugins checks
# plugins checks

AM_CONDITIONAL(HAVE_ID3LIB, false)
define([CHECK_MODULE_ID3LIB],
[
        AC_LMS_CHECK_PKG(ID3LIB, id3lib, [], [ID3LIB=false])
])

AM_CONDITIONAL(HAVE_VORBIS, false)
define([CHECK_MODULE_OGG],
[
        AC_LMS_CHECK_PKG(VORBIS, vorbis, [], [OGG=false])
])

# plugins declarations
AC_LMS_OPTIONAL_MODULE([dummy], true)
AC_LMS_OPTIONAL_MODULE([jpeg], true)
AC_LMS_OPTIONAL_MODULE([png], true)
AC_LMS_OPTIONAL_MODULE([id3lib], true, [CHECK_MODULE_ID3LIB])
AC_LMS_OPTIONAL_MODULE([video-dummy], true)
AC_LMS_OPTIONAL_MODULE([audio-dummy], true)
AC_LMS_OPTIONAL_MODULE([m3u], true)
AC_LMS_OPTIONAL_MODULE([ogg], true, [CHECK_MODULE_OGG])
AC_LMS_OPTIONAL_MODULE([pls], true)
AC_LMS_OPTIONAL_MODULE([asf], true)

AC_OUTPUT([
lightmediascanner.pc
Makefile
m4/Makefile
src/Makefile
src/bin/Makefile
src/lib/Makefile
src/plugins/Makefile
src/plugins/dummy/Makefile
src/plugins/jpeg/Makefile
src/plugins/png/Makefile
src/plugins/id3lib/Makefile
src/plugins/video-dummy/Makefile
src/plugins/audio-dummy/Makefile
src/plugins/m3u/Makefile
src/plugins/ogg/Makefile
src/plugins/pls/Makefile
src/plugins/asf/Makefile
])


# report
txt_strip() {
        echo "[$]@" | sed -e 's/^[[ \t]]*\([[^ \t]]*\)[[ \t]]*$/\1/g'
}

MODS=""
for mod in $OPTIONAL_MODULES; do
        MODS="$MODS ${COLOR_HGREEN}+$mod${COLOR_END}"
done
MODS=$(txt_strip $MODS)

UNUSED_MODS=""
for mod in $UNUSED_OPTIONAL_MODULES; do
        UNUSED_MODS="$UNUSED_MODS ${COLOR_HRED}-$mod${COLOR_END}"
done
UNUSED_MODS=$(txt_strip $UNUSED_MODS)

cat << SUMMARY_EOF

Summary:
 * project........: $PACKAGE $VERSION
 * prefix.........: $(txt_strip $prefix)
 * CFLAGS.........: $(txt_strip $CFLAGS)
SUMMARY_EOF

echo -e " * modules........: $MODS $UNUSED_MODS"
