@YELP_HELP_RULES@

HELP_ID = cafe-dictionary
HELP_FILES = index.docbook legal.xml

HELP_MEDIA =                                      \
	figures/cafe-dictionary-add-source.png        \
	figures/cafe-dictionary-applet.png            \
	figures/cafe-dictionary-applet-window.png     \
	figures/cafe-dictionary-application.png       \
	figures/cafe-dictionary-find.png              \
	figures/cafe-dictionary-lookup.png            \
	figures/cafe-dictionary-preferences-print.png \
	figures/cafe-dictionary-preferences-source.png

# Add linguas to be ignored, e.g. IGNORE_HELP_LINGUAS = ca de es fr
IGNORE_HELP_LINGUAS =
HELP_LINGUAS = $(if $(IGNORE_HELP_LINGUAS), \
	$(filter-out $(IGNORE_HELP_LINGUAS),$(subst /,,$(dir $(wildcard */*.po)))), \
	$(subst /,,$(dir $(wildcard */*.po))) )
