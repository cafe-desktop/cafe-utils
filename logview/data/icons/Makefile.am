NULL =

public_icons_themes = \
	hicolor	\
	$(NULL)

png_icons = \
        hicolor_apps_16x16_cafe-system-log.png \
        hicolor_apps_22x22_cafe-system-log.png \
        hicolor_apps_24x24_cafe-system-log.png \
        hicolor_apps_32x32_cafe-system-log.png \
        hicolor_apps_48x48_cafe-system-log.png \
        hicolor_apps_256x256_cafe-system-log.png \
	$(NULL)

public_icons = \
	$(png_icons) \
	hicolor_apps_scalable_cafe-system-log-symbolic.svg \
	$(NULL)

if HAVE_INKSCAPE
hicolor_apps_16x16_cafe-system-log.png: logview.svg
	$(if $(wildcard $(INKSCAPE)), \
		$(info $(shell $(INKSCAPE) -z $^ --export-area=303:65:319:81 -w=16 -h=16 -e=$@)), \
		$(error could not locate inkscape) \
	)

hicolor_apps_22x22_cafe-system-log.png: logview.svg
	$(if $(wildcard $(INKSCAPE)), \
		$(info $(shell $(INKSCAPE) -z $^ --export-area=303:101:325:123 -w=22 -h=22 -e=$@)), \
		$(error could not locate inkscape) \
	)

hicolor_apps_24x24_cafe-system-log.png: logview.svg
	$(if $(wildcard $(INKSCAPE)), \
		$(info $(shell $(INKSCAPE) -z $^ --export-area=302:100:326:124 -w=24 -h=24 -e=$@)), \
		$(error could not locate inkscape) \
	)

hicolor_apps_32x32_cafe-system-log.png: logview.svg
	$(if $(wildcard $(INKSCAPE)), \
		$(info $(shell $(INKSCAPE) -z $^ --export-area=304:142:336:174 -w=32 -h=32 -e=$@)), \
		$(error could not locate inkscape) \
	)

hicolor_apps_48x48_cafe-system-log.png: logview.svg
	$(if $(wildcard $(INKSCAPE)), \
		$(info $(shell $(INKSCAPE) -z $^ --export-area=296:202:344:250 -w=48 -h=48 -e=$@)), \
		$(error could not locate inkscape) \
	)

hicolor_apps_256x256_cafe-system-log.png: logview.svg
	$(if $(wildcard $(INKSCAPE)), \
		$(info $(shell $(INKSCAPE) -z $^ --export-area=33:11:289:267 -w=256 -h=256 -e=$@)), \
		$(error could not locate inkscape) \
	)
endif

noinst_DATA = \
	logview.svg \
	$(NULL)

EXTRA_DIST = \
	$(public_icons)		\
	$(noinst_DATA)		\
	$(NULL)

###############################################################################

install-icons: | $(public_icons)
	for icon in $(public_icons); do \
		THEME=`echo $$icon | cut -d_ -f1`; \
		CONTEXT=`echo $$icon | cut -d_ -f2`; \
		SIZE=`echo $$icon | cut -d_ -f3`; \
		ICONFILE=`echo $$icon | cut -d_ -f4`; \
		mkdir -p $(DESTDIR)$(datadir)/icons/$$THEME/$$SIZE/$$CONTEXT; \
		$(INSTALL_DATA) $(srcdir)/$$icon $(DESTDIR)$(datadir)/icons/$$THEME/$$SIZE/$$CONTEXT/$$ICONFILE; \
	done

uninstall-icons:
	-for icon in $(public_icons); do \
		THEME=`echo $$icon | cut -d_ -f1`; \
		CONTEXT=`echo $$icon | cut -d_ -f2`; \
		SIZE=`echo $$icon | cut -d_ -f3`; \
		ICONFILE=`echo $$icon | cut -d_ -f4`; \
		rm -f $(DESTDIR)$(datadir)/icons/$$THEME/$$SIZE/$$CONTEXT/$$ICONFILE; \
	done

install-data-local: install-icons

uninstall-local: uninstall-icons

ctk_update_icon_cache = ctk-update-icon-cache -f -t $(datadir)/icons/hicolor

install-data-hook: update-icon-cache
uninstall-hook: update-icon-cache
update-icon-cache:
	@-if test -z "$(DESTDIR)"; then \
		echo "Updating Ctk icon cache."; \
		$(ctk_update_icon_cache); \
	else \
		echo "*** Icon cache not updated.  After (un)install, run this:"; \
		echo "***   $(ctk_update_icon_cache)"; \
	fi

.PHONY: build-png-icons clean-png-icons
build-png-icons: | $(png_icons)

clean-png-icons:
	rm -f $(png_icons)
