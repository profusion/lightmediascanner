AC_INIT(lightmediascanner, 0.4.1.0, lms-devel@garage.maemo.org)
AC_PREREQ(2.52)
AC_CONFIG_SRCDIR(configure.ac)
AC_CANONICAL_BUILD
AC_CANONICAL_HOST
AC_ISC_POSIX

AM_INIT_AUTOMAKE(1.6 dist-bzip2)
AM_CONFIG_HEADER(config.h)
AC_SUBST(ACLOCAL_AMFLAGS, "-I m4")
_XTERM_COLORS
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])

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

AM_CONDITIONAL(HAVE_VORBIS, false)
define([CHECK_MODULE_OGG],
[
        AC_CHECK_HEADERS(tremor/ivorbiscodec.h tremor/ivorbisfile.h, HAVE_IVORBIS_HEADERS=yes, HAVE_IVORBIS_HEADERS=no)
        if test "x$HAVE_IVORBIS_HEADERS" = "xyes"; then
            AC_CHECK_LIB(vorbisidec, ogg_sync_bufferin, HAVE_IVORBIS_LIBS=yes, HAVE_IVORBIS_LIBS=no)
        fi

        AM_CONDITIONAL(USE_TREMOR, test "x$HAVE_IVORBIS_LIBS" = "xyes")
        if test "x$HAVE_IVORBIS_LIBS" = "xyes"; then
            AC_DEFINE(USE_TREMOR, 1, Define if libvorbisidec (aka tremor) support is enabled)
            VORBIS_LIBS="-lvorbisidec"
            AC_SUBST(VORBIS_LIBS)
        else
            AC_LMS_CHECK_PKG(VORBIS, vorbis, [], [OGG=false])
        fi
])

AM_CONDITIONAL(HAVE_MP4V2, false)
define([CHECK_MODULE_MP4],
[
        AC_CHECK_HEADERS(mp4.h, HAVE_MP4_HEADERS=yes, HAVE_MP4_HEADERS=no)
        AC_CHECK_HEADERS(mp4v2/mp4v2.h, HAVE_MP4V2_HEADERS=yes, HAVE_MP4V2_HEADERS=no)
        if test "x$HAVE_MP4_HEADERS" = "xyes" -o "x$HAVE_MP4V2_HEADERS" = "xyes"; then
            AC_CHECK_LIB(mp4v2, MP4Read, [MP4=true], [MP4=false])
            MP4V2_LIBS="-lmp4v2"
            AC_SUBST(MP4V2_LIBS)
            if test "x$HAVE_MP4V2_HEADERS" = "xyes"; then
               AC_DEFINE(HAVE_MP4V2, 1, Define if mp4v2.h is present)
            fi
            if test "x$HAVE_MP4_HEADERS" = "xyes"; then
               AC_DEFINE(HAVE_MP4, 1, Define if mp4.h is present)
            fi
        else
            MP4=false
        fi
])

AM_CONDITIONAL(HAVE_FLAC, false)
define([CHECK_MODULE_FLAC],
[
        AC_LMS_CHECK_PKG(FLAC, flac, [], [FLAC=false])
])

# plugins declarations
AC_LMS_OPTIONAL_MODULE([dummy], true)
AC_LMS_OPTIONAL_MODULE([jpeg], true)
AC_LMS_OPTIONAL_MODULE([png], true)
AC_LMS_OPTIONAL_MODULE([video-dummy], true)
AC_LMS_OPTIONAL_MODULE([audio-dummy], true)
AC_LMS_OPTIONAL_MODULE([m3u], true)
AC_LMS_OPTIONAL_MODULE([ogg], true, [CHECK_MODULE_OGG])
AC_LMS_OPTIONAL_MODULE([pls], true)
AC_LMS_OPTIONAL_MODULE([asf], true)
AC_LMS_OPTIONAL_MODULE([rm], true)
AC_LMS_OPTIONAL_MODULE([mp4], true, [CHECK_MODULE_MP4])
AC_LMS_OPTIONAL_MODULE([id3], true)
AC_LMS_OPTIONAL_MODULE([flac], true, [CHECK_MODULE_FLAC])

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
src/plugins/video-dummy/Makefile
src/plugins/audio-dummy/Makefile
src/plugins/m3u/Makefile
src/plugins/ogg/Makefile
src/plugins/pls/Makefile
src/plugins/asf/Makefile
src/plugins/rm/Makefile
src/plugins/mp4/Makefile
src/plugins/id3/Makefile
src/plugins/flac/Makefile
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
