AC_INIT(lightmediascanner, 0.4.5.0, barbieri@profusion.mobi)
AC_PREREQ(2.60)
AC_CONFIG_SRCDIR(configure.ac)
AC_CANONICAL_BUILD
AC_CANONICAL_HOST
AC_ISC_POSIX

AM_INIT_AUTOMAKE(foreign 1.11 silent-rules subdir-objects dist-bzip2)
AM_CONFIG_HEADER(config.h)
AC_USE_SYSTEM_EXTENSIONS
AC_CONFIG_MACRO_DIR([m4])
_XTERM_COLORS
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])

AC_PROG_CC
AM_PROG_CC_STDC
AM_PROG_CC_C_O
AC_HEADER_STDC
AC_C_CONST

define([AC_LIBTOOL_LANG_F77_CONFIG], [:])dnl
LT_INIT([disable-static])

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
AM_ICONV

# required modules
PKG_CHECK_MODULES(SQLITE3, [sqlite3 >= 3.3])

# plugins checks

AM_CONDITIONAL(HAVE_VORBIS, false)
AM_CONDITIONAL(USE_TREMOR, false)
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
	AC_LMS_CHECK_PKG(THEORADEC, theoradec, [], [OGG=false])
])

AM_CONDITIONAL(HAVE_MP4V2, false)
define([CHECK_MODULE_MP4],
[
        AC_CHECK_HEADERS(mp4v2/mp4v2.h, HAVE_MP4V2_HEADERS=yes, HAVE_MP4V2_HEADERS=no)
        if test "x$HAVE_MP4V2_HEADERS" = "xyes"; then
            AC_CHECK_LIB(mp4v2, MP4Read, [MP4=true], [MP4=false])
            MP4V2_LIBS="-lmp4v2"
            AC_SUBST(MP4V2_LIBS)

            # strict about new 2.0 api
            AC_MSG_CHECKING([mp4v2 2.0 API (MP4Read and MP4Close)])
            AC_COMPILE_IFELSE([AC_LANG_PROGRAM(
               [[#include <mp4v2/mp4v2.h>]],
               [[MP4FileHandle fh = MP4Read("/tmp");
                 MP4Close(fh, 0);
               ]])],
               [
                  MP4=true
                  AC_MSG_RESULT([yes])
               ], [
                  MP4=false
                  AC_MSG_RESULT([no])])
        else
            MP4=false
        fi
])

AM_CONDITIONAL(HAVE_FLAC, false)
define([CHECK_MODULE_FLAC],
[
        AC_LMS_CHECK_PKG(FLAC, flac, [], [FLAC=false])
])

AM_CONDITIONAL(HAVE_GENERIC, false)
define([CHECK_MODULE_GENERIC],
[
        AC_LMS_CHECK_PKG(GENERIC, [libavcodec libavformat], [], [GENERIC=false])
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
AC_LMS_OPTIONAL_MODULE([wave], true)
AC_LMS_OPTIONAL_MODULE([generic], true, [CHECK_MODULE_GENERIC])


AC_ARG_ENABLE([daemon],
        [AC_HELP_STRING([--disable-daemon],
                [Disable DBus scanner daemon. @<:@default=enable@:>@])],
        [build_daemon=${enableval}], [build_daemon=yes])

dbusservicedir="${datadir}/dbus-1/services"
AC_ARG_WITH([dbus-services],
        [AC_HELP_STRING([--with-dbus-services=DBUS_SERVICES],
                [Specify a directory to store dbus service files.])],
        [dbusservicedir=$withval])
AC_SUBST(dbusservicedir)

dbusdir=""
if test "$build_daemon" = "yes"; then
        PKG_CHECK_MODULES(GIO, [gio-2.0 >= 2.32])
fi
AM_CONDITIONAL([BUILD_DAEMON], [test "$build_daemon" = "yes"])


#####################################################################
# Default CFLAGS and LDFLAGS
#####################################################################

CC_CHECK_FLAGS_APPEND(with_cflags, [CFLAGS], [\
		       -pipe \
		       -DANOTHER_BRICK_IN_THE \
		       -Wall \
		       -W \
		       -Wextra \
		       -Wno-inline \
		       -Wvla \
		       -Wundef \
		       -Wformat=2 \
		       -Wlogical-op \
		       -Wsign-compare \
		       -Wformat-security \
		       -Wmissing-include-dirs \
		       -Wformat-nonliteral \
		       -Wold-style-definition \
		       -Wpointer-arith \
		       -Winit-self \
		       -Wdeclaration-after-statement \
		       -Wfloat-equal \
		       -Wmissing-prototypes \
		       -Wstrict-prototypes \
		       -Wredundant-decls \
		       -Wmissing-declarations \
		       -Wmissing-noreturn \
		       -Wshadow \
		       -Wendif-labels \
		       -Wstrict-aliasing=2 \
		       -Wwrite-strings \
		       -Wno-long-long \
		       -Wno-overlength-strings \
		       -Wno-unused-parameter \
		       -Wno-missing-field-initializers \
		       -Wno-unused-result \
		       -Wnested-externs \
		       -Wchar-subscripts \
		       -Wtype-limits \
		       -Wuninitialized \
		       -fno-common \
		       -fdiagnostics-show-option \
		       -fvisibility=hidden \
		       -ffunction-sections \
		       -fdata-sections])
AC_SUBST([WARNINGFLAGS], $with_cflags)


CC_CHECK_FLAGS_APPEND([with_ldflags], [LDFLAGS], [ \
		       -Wl,--as-needed \
		       -Wl,--gc-sections])
AC_SUBST([GCLDFLAGS], $with_ldflags)


AC_OUTPUT([
lightmediascanner.pc
lightmediascanner.spec
org.lightmediascanner.service
Makefile
m4/Makefile
src/Makefile
src/bin/Makefile
src/lib/Makefile
src/plugins/Makefile
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
echo -e " * daemon.........: ${build_daemon}"
