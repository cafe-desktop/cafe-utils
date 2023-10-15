SUBDIRS=icons

desktopdir = $(datadir)/applications
desktop_in_files = cafe-system-log.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)
$(desktop_in_files): $(desktop_in_files:.desktop.in=.desktop.in.in)
	@sed -e "s|\@VERSION\@|@VERSION@|" $< > $@
$(desktop_DATA): $(desktop_in_files)
	$(AM_V_GEN) $(MSGFMT) --desktop --template $< -d $(top_srcdir)/po -o $@

man_MANS = cafe-system-log.1

gsettings_SCHEMAS = org.cafe.system-log.gschema.xml
@GSETTINGS_RULES@

EXTRA_DIST = 			\
	logview-filter.ui	\
	logview-toolbar.xml	\
	cafe-system-log.desktop.in.in	\
	$(man_MANS)		\
	org.cafe.system-log.gresource.xml

DISTCLEANFILES =		\
	$(desktop_DATA)		\
	$(desktop_in_files)	\
	$(gsettings_SCHEMAS)
