NULL =

man_MANS = cafe-dictionary.1

gdictappdir = $(datadir)/applications
gdictapp_in_files = cafe-dictionary.desktop.in
gdictapp_DATA = $(gdictapp_in_files:.desktop.in=.desktop)

$(gdictapp_in_files): $(gdictapp_in_files:.desktop.in=.desktop.in.in)
	$(AM_V_GEN)sed -e "s|\@VERSION\@|@VERSION@|" $< > $@

# dictionary sources definitions
dictsourcedir = $(datadir)/cafe-dict/sources
dictsource_in_files = \
	default.desktop.in \
	thai.desktop.in \
	$(NULL)
dictsource_DATA = $(dictsource_in_files:.desktop.in=.desktop)

%.desktop: %.desktop.in
	$(AM_V_GEN) $(MSGFMT) --desktop --template $< -d $(top_srcdir)/po -o $@

builderdir = $(datadir)/cafe-dictionary
builder_DATA = \
	cafe-dictionary-preferences.ui	\
	cafe-dictionary-source.ui	\
	$(NULL)

uidir = $(datadir)/cafe-dictionary
ui_DATA = cafe-dictionary-ui.xml

if BUILD_GDICT_APPLET

# applet file, dbus service, and menu definition
appletdir = $(datadir)/cafe-panel/applets
applet_in_files = org.cafe.DictionaryApplet.cafe-panel-applet.desktop.in
applet_DATA = $(applet_in_files:.cafe-panel-applet.desktop.in=.cafe-panel-applet)

$(applet_in_files): $(applet_in_files).in Makefile
	$(AM_V_GEN)sed \
		-e "s|\@LIBEXECDIR\@|$(libexecdir)|" \
		$< > $@

$(applet_DATA): $(applet_in_files) Makefile
	$(AM_V_GEN) $(MSGFMT) --desktop --keyword=Name --keyword=Description --template $< -d $(top_srcdir)/po -o $@

servicedir = $(datadir)/dbus-1/services
service_in_files = org.cafe.panel.applet.DictionaryAppletFactory.service.in
service_DATA = $(service_in_files:.service.in=.service)

org.cafe.panel.applet.DictionaryAppletFactory.service: $(service_in_files)
	$(AM_V_GEN)sed \
		-e "s|\@LIBEXECDIR\@|$(libexecdir)|" \
		$< > $@

ui_DATA += dictionary-applet-menu.xml

endif # BUILD_GDICT_APPLET

appdatadir = $(datadir)/metainfo
appdata_in_files = cafe-dictionary.appdata.xml.in
appdata_DATA = $(appdata_in_files:.xml.in=.xml)
$(appdata_DATA): $(appdata_in_files)
	$(AM_V_GEN) $(MSGFMT) --xml --template $< -d $(top_srcdir)/po -o $@

gsettings_SCHEMAS = org.cafe.dictionary.gschema.xml
@GSETTINGS_RULES@

EXTRA_DIST = \
	$(applet_in_files).in \
	$(service_in_files) \
	$(gdictapp_in_files).in \
	$(dictsource_in_files) \
	$(man_MANS) \
	$(appdata_in_files) \
	$(builder_DATA) \
	$(ui_DATA) \
	$(NULL)

CLEANFILES = \
	$(applet_DATA) \
	$(applet_DATA).in \
	$(applet_in_files) \
	$(service_DATA) \
	$(dictsource_DATA) \
	$(gdictapp_in_files) \
	$(gdictapp_DATA) \
	$(gsettings_SCHEMAS) \
	cafe-dictionary.appdata.xml \
	$(NULL)
