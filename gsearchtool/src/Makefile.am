NULL =

AM_CPPFLAGS = \
	-I$(top_srcdir)/gsearchtool/libeggsmclient \
	-I$(top_srcdir)/gsearchtool/libcafeui-deprecated \
	-DCAFELOCALEDIR=\""$(datadir)/locale"\" \
	-DLOCALEDIR=\"$(datadir)/locale\" \
	-DDATADIR=\""$(datadir)"\" \
	-DGREP_COMMAND=\""$(GREP_COMMAND)"\"

libeggsmclient_LIB = $(top_builddir)/gsearchtool/libeggsmclient/libeggsmclient.la
libcafeui_deprecated_LIB = $(top_builddir)/gsearchtool/libcafeui-deprecated/libcafeui-deprecated.la

bin_PROGRAMS = cafe-search-tool

cafe_search_tool_SOURCES = \
	gsearchtool-support.c \
	gsearchtool-support.h \
	gsearchtool-callbacks.c \
	gsearchtool-callbacks.h \
	gsearchtool.c \
	gsearchtool.h

cafe_search_tool_CFLAGS = \
	$(GLIB_CFLAGS) \
	$(GIO_CFLAGS) \
	$(GIO_UNIX_CFLAGS) \
	$(GTK_CFLAGS) \
	$(WARN_CFLAGS) \
	$(NULL)

cafe_search_tool_LDADD = \
	$(GLIB_LIBS) \
	$(GIO_LIBS) \
	$(GIO_UNIX_LIBS) \
	$(GTK_LIBS) \
	$(libeggsmclient_LIB) \
	$(libcafeui_deprecated_LIB)

