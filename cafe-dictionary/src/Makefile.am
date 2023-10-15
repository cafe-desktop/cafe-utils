NULL =

AM_CPPFLAGS = \
	-DPREFIX=\""$(prefix)"\"			\
	-DSYSCONFDIR=\""$(sysconfdir)"\"		\
	-DLIBDIR=\""$(libdir)"\"			\
	-DDATADIR=\""$(datadir)"\" 			\
	-DPKGDATADIR=\""$(datadir)/cafe-dictionary"\" 	\
	-DCAFELOCALEDIR=\""$(datadir)/locale"\"		\
	$(NULL)

bin_PROGRAMS = cafe-dictionary

if BUILD_GDICT_APPLET
libexec_PROGRAMS = cafe-dictionary-applet
endif

cafe_dictionary_SOURCES = \
	gdict-about.c		\
	gdict-about.h		\
	gdict-app.c		\
	gdict-app.h		\
	gdict-common.c 		\
	gdict-common.h		\
	gdict-pref-dialog.c	\
	gdict-pref-dialog.h	\
	gdict-print.c 		\
	gdict-print.h 		\
	gdict-sidebar.c		\
	gdict-sidebar.h		\
	gdict-source-dialog.c 	\
	gdict-source-dialog.h 	\
	gdict-window.c 		\
	gdict-window.h 		\
	main.c			\
	$(NULL)

cafe_dictionary_CFLAGS = \
	-I$(top_builddir)/cafe-dictionary \
	-I$(top_srcdir)/cafe-dictionary \
	$(GLIB_CFLAGS) \
	$(GIO_CFLAGS) \
	$(GTK_CFLAGS) \
	$(WARN_CFLAGS) \
	$(NULL)

cafe_dictionary_LDADD = \
	-lm			\
	$(top_builddir)/cafe-dictionary/libgdict/libcafedict.la \
	$(GLIB_LIBS) \
	$(GIO_LIBS) \
	$(GTK_LIBS) \
	$(NULL)

if BUILD_GDICT_APPLET

cafe_dictionary_applet_SOURCES = \
	gdict-about.c 		\
	gdict-about.h		\
	gdict-aligned-window.c	\
	gdict-aligned-window.h	\
	gdict-applet.c		\
	gdict-applet.h		\
	gdict-common.c		\
	gdict-common.h		\
	gdict-pref-dialog.c	\
	gdict-pref-dialog.h	\
	gdict-print.c 		\
	gdict-print.h 		\
	gdict-sidebar.c		\
	gdict-sidebar.h		\
	gdict-source-dialog.c 	\
	gdict-source-dialog.h 	\
	$(NULL)

cafe_dictionary_applet_CFLAGS = \
	-I$(top_builddir)/cafe-dictionary \
	-I$(top_srcdir)/cafe-dictionary \
	$(APPLET_CFLAGS) \
	$(GLIB_CFLAGS) \
	$(GIO_CFLAGS) \
	$(GTK_CFLAGS) \
	$(WARN_CFLAGS) \
	$(NULL)

cafe_dictionary_applet_LDADD = \
	-lm			\
	$(top_builddir)/cafe-dictionary/libgdict/libcafedict.la \
	$(APPLET_LIBS) \
	$(GLIB_LIBS) \
	$(GIO_LIBS) \
	$(GTK_LIBS) \
	$(NULL)

endif # BUILD_GDICT_APPLET
