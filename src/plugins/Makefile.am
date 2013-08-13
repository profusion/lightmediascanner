MAINTAINERCLEANFILES = Makefile.in

AM_LDFLAGS = -module -avoid-version -shared $(GCLDFLAGS)
AM_CFLAGS = -fvisibility=hidden $(WARNINGFLAGS)
AM_CPPFLAGS = -include $(top_builddir)/config.h \
		-I$(top_srcdir)/src/lib @SQLITE3_CFLAGS@
pkgdir = $(pluginsdir)

PLUGINS_LIBADD = $(top_builddir)/src/lib/liblightmediascanner.la

pkg_LTLIBRARIES =
BUILT_SOURCES =
EXTRA_DIST =
SUBDIRS =
noinst_HEADERS =

if USE_MODULE_DUMMY
pkg_LTLIBRARIES += dummy/dummy.la
dummy_dummy_la_SOURCES = dummy/dummy.c
dummy_dummy_la_LIBADD = $(PLUGINS_LIBADD)
endif

if USE_MODULE_JPEG
pkg_LTLIBRARIES += jpeg/jpeg.la
jpeg_jpeg_la_SOURCES = jpeg/jpeg.c
jpeg_jpeg_la_LIBADD = $(PLUGINS_LIBADD)
endif

if USE_MODULE_PNG
pkg_LTLIBRARIES += png/png.la
png_png_la_SOURCES = png/png.c
png_png_la_LIBADD = $(PLUGINS_LIBADD)
endif

if USE_MODULE_VIDEO_DUMMY
pkg_LTLIBRARIES += video-dummy/video-dummy.la
video_dummy_video_dummy_la_SOURCES = video-dummy/video-dummy.c
video_dummy_video_dummy_la_LIBADD = $(PLUGINS_LIBADD)
endif

if USE_MODULE_AUDIO_DUMMY
pkg_LTLIBRARIES += audio-dummy/audio-dummy.la
audio_dummy_audio_dummy_la_SOURCES = audio-dummy/audio-dummy.c
audio_dummy_audio_dummy_la_LIBADD = $(PLUGINS_LIBADD)
endif

if USE_MODULE_M3U
pkg_LTLIBRARIES += m3u/m3u.la
m3u_m3u_la_SOURCES = m3u/m3u.c
m3u_m3u_la_LIBADD = $(PLUGINS_LIBADD)
endif

if USE_MODULE_PLS
pkg_LTLIBRARIES += pls/pls.la
pls_pls_la_SOURCES = pls/pls.c
pls_pls_la_LIBADD = $(PLUGINS_LIBADD)
endif

if USE_MODULE_OGG
pkg_LTLIBRARIES += ogg/ogg.la
ogg_ogg_la_SOURCES = ogg/ogg.c
ogg_ogg_la_LIBADD = $(PLUGINS_LIBADD) $(VORBIS_LIBS)
noinst_HEADERS += ogg/lms_ogg_private.h

if USE_TREMOR
ogg_ogg_la_SOURCES += ogg/lms_ogg_tremor.c
else
ogg_ogg_la_SOURCES += ogg/lms_ogg_vorbis.c
endif

endif

if USE_MODULE_ASF
pkg_LTLIBRARIES += asf/asf.la
asf_asf_la_SOURCES = asf/asf.c
asf_asf_la_LIBADD = $(PLUGINS_LIBADD)
endif

if USE_MODULE_RM
pkg_LTLIBRARIES += rm/rm.la
rm_rm_la_SOURCES = rm/rm.c
rm_rm_la_LIBADD = $(PLUGINS_LIBADD)
endif

if USE_MODULE_MP4
pkg_LTLIBRARIES += mp4/mp4.la
mp4_mp4_la_SOURCES = mp4/mp4.c
mp4_mp4_la_LIBADD = $(PLUGINS_LIBADD) $(MP4V2_LIBS)
endif

if USE_MODULE_ID3
pkg_LTLIBRARIES += id3/id3.la
id3_id3_la_SOURCES = id3/id3.c id3/id3v1_genres.c
id3_id3_la_LIBADD = $(PLUGINS_LIBADD) $(MP4V2_LIBS)

id3/id3v1_genres.c: $(srcdir)/id3/id3v1_genres.def $(srcdir)/id3/id3v1_genres_gen.awk
	$(AWK) -f $(srcdir)/id3/id3v1_genres_gen.awk $(srcdir)/id3/id3v1_genres.def > $@

EXTRA_DIST += id3/id3v1_genres.def id3/id3v1_genres_gen.awk
BUILT_SOURCES += id3/id3v1_genres.c

endif

if USE_MODULE_FLAC
pkg_LTLIBRARIES += flac/flac.la
flac_flac_la_SOURCES = flac/flac.c
flac_flac_la_LIBADD = $(PLUGINS_LIBADD) $(FLAC_LIBS)
endif

if USE_MODULE_WAVE
pkg_LTLIBRARIES += wave/wave.la
wave_wave_la_SOURCES = wave/wave.c
wave_wave_la_LIBADD = $(PLUGINS_LIBADD) $(FLAC_LIBS)
endif

CLEAN_FILES = $(BUILT_SOURCES)
