/* gdict-common.h - shared code between application and applet
 *
 * This file is part of CAFE Dictionary
 *
 * Copyright (C) 2005 Emmanuele Bassi
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/types.h>
#include <errno.h>

#include <glib.h>
#include <glib/gi18n.h>
#include <glib/gstdio.h>

#include <ctk/ctk.h>

#include "gdict-common.h"

gchar *
gdict_get_data_dir (void)
{
  gchar *retval;

  retval = g_build_filename (g_get_user_config_dir (),
		  	     "cafe",
			     "cafe-dictionary",
			     NULL);

  return retval;
}

/* create the data directory inside $HOME, if it doesn't exist yet */
gboolean
gdict_create_data_dir (void)
{
  gchar *data_dir_name;

  data_dir_name = gdict_get_data_dir ();
  if (g_mkdir_with_parents (data_dir_name, 0700) == -1)
    {
      /* this is weird, but sometimes there's a "cafe-dictionary" file
       * inside $HOME/.cafe2; see bug #329126.
       */
      if ((errno == EEXIST) &&
          (g_file_test (data_dir_name, G_FILE_TEST_IS_REGULAR)))
        {
          gchar *backup = g_strdup_printf ("%s.pre-2-14", data_dir_name);

	  if (g_rename (data_dir_name, backup) == -1)
	    {
              GtkWidget *error_dialog;

	      error_dialog = ctk_message_dialog_new (NULL,
                                                     CTK_DIALOG_MODAL,
						     CTK_MESSAGE_ERROR,
						     CTK_BUTTONS_CLOSE,
						     _("Unable to rename file '%s' to '%s': %s"),
						     data_dir_name,
						     backup,
						     g_strerror (errno));

	      ctk_dialog_run (CTK_DIALOG (error_dialog));

	      ctk_widget_destroy (error_dialog);
	      g_free (backup);
	      g_free (data_dir_name);

	      return FALSE;
            }

	  g_free (backup);

          if (g_mkdir_with_parents (data_dir_name, 0700) == -1)
            {
              GtkWidget *error_dialog;

	      error_dialog = ctk_message_dialog_new (NULL,
						     CTK_DIALOG_MODAL,
						     CTK_MESSAGE_ERROR,
						     CTK_BUTTONS_CLOSE,
						     _("Unable to create the data directory '%s': %s"),
						     data_dir_name,
						     g_strerror (errno));

	      ctk_dialog_run (CTK_DIALOG (error_dialog));

	      ctk_widget_destroy (error_dialog);
              g_free (data_dir_name);

	      return FALSE;
            }

	  goto success;
	}

      if (errno != EEXIST)
        {
          GtkWidget *error_dialog;

	  error_dialog = ctk_message_dialog_new (NULL,
						 CTK_DIALOG_MODAL,
						 CTK_MESSAGE_ERROR,
						 CTK_BUTTONS_CLOSE,
						 _("Unable to create the data directory '%s': %s"),
						 data_dir_name,
						 g_strerror (errno));

	  ctk_dialog_run (CTK_DIALOG (error_dialog));

	  ctk_widget_destroy (error_dialog);
	  g_free (data_dir_name);

	  return FALSE;
	}
    }

success:
  g_free (data_dir_name);

  return TRUE;
}

/* shows an error dialog making it transient for @parent */
void
gdict_show_error_dialog (GtkWindow   *parent,
			 const gchar *message,
			 const gchar *detail)
{
  GtkWidget *dialog;

  g_return_if_fail ((parent == NULL) || (CTK_IS_WINDOW (parent)));
  g_return_if_fail (message != NULL);

  dialog = ctk_message_dialog_new (parent,
  				   CTK_DIALOG_DESTROY_WITH_PARENT,
  				   CTK_MESSAGE_ERROR,
  				   CTK_BUTTONS_OK,
  				   "%s", message);
  ctk_window_set_title (CTK_WINDOW (dialog), "");

  if (detail)
    ctk_message_dialog_format_secondary_text (CTK_MESSAGE_DIALOG (dialog),
  					      "%s", detail);

  if (parent && ctk_window_get_group (parent))
    ctk_window_group_add_window (ctk_window_get_group (parent), CTK_WINDOW (dialog));

  ctk_dialog_run (CTK_DIALOG (dialog));

  ctk_widget_destroy (dialog);
}

void
gdict_show_gerror_dialog (GtkWindow   *parent,
			  const gchar *message,
			  GError      *error)
{
  g_return_if_fail ((parent == NULL) || (CTK_IS_WINDOW (parent)));
  g_return_if_fail (message != NULL);
  g_return_if_fail (error != NULL);

  gdict_show_error_dialog (parent, message, error->message);

  g_error_free (error);
  error = NULL;
}
