NULL =

bin_PROGRAMS = cafe-disk-image-mounter

cafe_disk_image_mounter_SOURCES = 			\
	main.c						\
	$(NULL)

cafe_disk_image_mounter_CPPFLAGS = 			\
	-I.						\
	-I$(srcdir)					\
	-DG_LOG_DOMAIN=\"cafe-disk-image-mounter\"	\
	-DCAFELOCALEDIR=\""$(datadir)/locale"\"		\
	$(NULL)

cafe_disk_image_mounter_CFLAGS = 			\
	$(GLIB_CFLAGS)					\
	$(UDISKS2_CFLAGS)				\
	$(CTK_CFLAGS)					\
	$(WARN_CFLAGS)					\
	$(NULL)

cafe_disk_image_mounter_LDADD = 			\
	$(GLIB_LIBS)					\
	$(UDISKS2_LIBS)					\
	$(CTK_LIBS)					\
	$(NULL)
