@YELP_HELP_RULES@

HELP_ID = cafe-search-tool
HELP_FILES = index.docbook legal.xml

HELP_MEDIA = figures/cafe-search-tool_window.png

# Add linguas to be ignored, e.g. IGNORE_HELP_LINGUAS = ca de es fr
IGNORE_HELP_LINGUAS =
HELP_LINGUAS = $(if $(IGNORE_HELP_LINGUAS), \
	$(filter-out $(IGNORE_HELP_LINGUAS),$(subst /,,$(dir $(wildcard */*.po)))), \
	$(subst /,,$(dir $(wildcard */*.po))) )
