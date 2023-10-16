NULL =

AM_CPPFLAGS =						\
	-I.						\
	-I$(srcdir)					\
	-DCAFELOCALEDIR=\""$(datadir)/locale"\"		\
	$(NULL)

bin_PROGRAMS = cafe-screenshot

BUILT_SOURCES =				\
	screenshot-resources.c		\
	screenshot-resources.h		\
	$(NULL)

nodist_cafe_screenshot_SOURCES =	\
	$(BUILT_SOURCES)		\
	$(NULL)

cafe_screenshot_SOURCES =		\
	cafe-screenshot.c		\
	screenshot-dialog.c		\
	screenshot-dialog.h		\
	screenshot-shadow.c		\
	screenshot-shadow.h		\
	screenshot-utils.c		\
	screenshot-utils.h		\
	screenshot-save.c		\
	screenshot-save.h		\
	screenshot-xfer.c		\
	screenshot-xfer.h

cafe_screenshot_CFLAGS =		\
	$(GLIB_CFLAGS)			\
	$(GIO_CFLAGS)			\
	$(LIBKANBERRA_CTK_CFLAGS)	\
	$(GTHREAD_CFLAGS)		\
	$(CTK_CFLAGS)			\
	$(WARN_CFLAGS)			\
	$(NULL)

cafe_screenshot_LDFLAGS = -export-dynamic

cafe_screenshot_LDADD =			\
	$(XSHAPE_LIBS)			\
	$(GLIB_LIBS)			\
	$(GIO_LIBS)			\
	$(LIBKANBERRA_CTK_LIBS)		\
	$(GTHREAD_LIBS)			\
	$(CTK_LIBS)			\
	-lm

screenshot-resources.h screenshot-resources.c: $(srcdir)/../data/org.cafe.screenshot.gresource.xml Makefile $(shell $(GLIB_COMPILE_RESOURCES) --generate-dependencies --sourcedir $(srcdir)/../data $(srcdir)/../data/org.cafe.screenshot.gresource.xml)
	$(AM_V_GEN) XMLLINT=$(XMLLINT) $(GLIB_COMPILE_RESOURCES) --target $@ --sourcedir $(srcdir)/../data --generate --c-name screenshot $<

CLEANFILES =				\
	$(BUILT_SOURCES)		\
	$(NULL)

install-exec-local:
	rm -f $(DESTDIR)$(bindir)/cafe-panel-screenshot
	ln -s cafe-screenshot $(DESTDIR)$(bindir)/cafe-panel-screenshot

