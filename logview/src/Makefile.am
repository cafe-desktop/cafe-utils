SUBDIRS = tests

AM_CPPFLAGS =						\
	-DG_LOG_DOMAIN=\"cafe-system-log\"		\
	-DCAFELOCALEDIR=\""$(datadir)/locale"\"		\
	-DDATADIR=\""$(datadir)"\"			\
	-DLOGVIEWINSTALLPREFIX=\""$(prefix)/\""		\
	$(NULL)

bin_PROGRAMS = cafe-system-log

BUILT_SOURCES = 		\
	logview-marshal.c	\
	logview-marshal.h	\
	logview-resources.c	\
	logview-resources.h

nodist_cafe_system_log_SOURCES = \
	$(BUILT_SOURCES)

cafe_system_log_SOURCES = 	\
	logview-app.c		\
	logview-app.h		\
	logview-main.c		\
	logview-about.h		\
	logview-manager.c	\
	logview-manager.h	\
	logview-utils.c		\
	logview-utils.h		\
	logview-loglist.c	\
	logview-loglist.h	\
	logview-window.c	\
	logview-window.h	\
	logview-log.h		\
	logview-log.c		\
	logview-findbar.h	\
	logview-findbar.c	\
	logview-prefs.c		\
	logview-prefs.h		\
	logview-filter.h	\
	logview-filter.c	\
	logview-filter-manager.h \
	logview-filter-manager.c

cafe_system_log_CFLAGS =	\
	$(GLIB_CFLAGS)		\
	$(GTHREAD_CFLAGS)	\
	$(GIO_CFLAGS)		\
	$(GTK_CFLAGS)		\
	$(WARN_CFLAGS)		\
	$(NULL)

cafe_system_log_LDADD =		\
	$(GLIB_LIBS)		\
	$(GIO_LIBS)		\
	$(GTHREAD_LIBS)		\
	$(GTK_LIBS)		\
	$(Z_LIBS)		\
	-lm

logview-marshal.h: logview-marshal.list $(GLIB_GENMARSHAL)
	$(GLIB_GENMARSHAL) $< --header --prefix=logview_marshal >> $@

logview-marshal.c: logview-marshal.list $(GLIB_GENMARSHAL)
	echo "#include \"logview-marshal.h\"" > $@ && \
	$(GLIB_GENMARSHAL) $< --body --prefix=logview_marshal >> $@

logview-resources.h logview-resources.c: $(srcdir)/../data/org.cafe.system-log.gresource.xml Makefile $(shell $(GLIB_COMPILE_RESOURCES) --generate-dependencies --sourcedir $(srcdir)/../data $(srcdir)/../data/org.cafe.system-log.gresource.xml)
	$(AM_V_GEN) XMLLINT=$(XMLLINT) $(GLIB_COMPILE_RESOURCES) --target $@ --sourcedir $(srcdir)/../data --generate --c-name logview $<

EXTRA_DIST = logview-marshal.list

CLEANFILES =			\
	$(BUILT_SOURCES)

dist-hook:
	cd $(distdir) ; rm -f $(CLEANFILES)

