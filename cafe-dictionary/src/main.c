#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <libintl.h>

#include "gdict-app.h"

int main (int argc, char *argv[])
{
	bindtextdomain (GETTEXT_PACKAGE, CAFELOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	gdict_init (&argc, &argv);

	g_object_set (ctk_settings_get_default (), "ctk-menu-images", TRUE, NULL);
	g_object_set (ctk_settings_get_default (), "ctk-button-images", TRUE, NULL);

	gdict_main ();

	gdict_cleanup ();

	return EXIT_SUCCESS;
}
