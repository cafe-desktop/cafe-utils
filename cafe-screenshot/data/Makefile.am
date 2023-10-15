cafe_screenshotdir  = $(datadir)/applications
cafe_screenshot_in_files = cafe-screenshot.desktop.in
cafe_screenshot_DATA = $(cafe_screenshot_in_files:.desktop.in=.desktop)
$(cafe_screenshot_DATA): $(cafe_screenshot_in_files)
	$(AM_V_GEN) $(MSGFMT) --desktop --template $< -d $(top_srcdir)/po -o $@

man_MANS = cafe-screenshot.1

appdatadir = $(datadir)/metainfo
appdata_in_files = cafe-screenshot.appdata.xml.in
appdata_DATA = $(appdata_in_files:.xml.in=.xml)
$(appdata_DATA): $(appdata_in_files)
	$(AM_V_GEN) $(MSGFMT) --xml --template $< -d $(top_srcdir)/po -o $@

gsettings_SCHEMAS = org.cafe.screenshot.gschema.xml
@GSETTINGS_RULES@

EXTRA_DIST =					\
	$(appdata_in_files)			\
	$(cafe_screenshot_in_files)		\
	$(man_MANS)				\
	cafe-screenshot.ui			\
	org.cafe.screenshot.gresource.xml

CLEANFILES = \
	$(gsettings_SCHEMAS)	\
	$(cafe_screenshot_DATA)	\
	$(appdata_DATA)

dist-hook:
	cd $(distdir) ; rm -f $(CLEANFILES)

install-exec-local:
	rm -f $(DESTDIR)$(mandir)/man1/cafe-panel-screenshot.1
	ln -s cafe-screenshot.1 $(DESTDIR)$(mandir)/man1/cafe-panel-screenshot.1

