/* Baobab - (C) 2005 Fabio Marzocca

	baobab-remote-connect-dialog.c

   Modified module from baul-connect-server-dialog.c
   Released under same licence
 */
/*
 * Baul
 *
 * Copyright (C) 2003 Red Hat, Inc.
 *
 * Baul is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * Baul is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; see the file COPYING.  If not,
 * write to the Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include <config.h>
#include "baobab-remote-connect-dialog.h"

#include <string.h>
#include <glib/gi18n.h>
#include <gio/gio.h>
#include <ctk/ctk.h>

#include "baobab.h"


/* Translators: all the strings in this file are meant to map the
   similar strings inside baul-connect-server and should be
   translated the same way
*/

struct _BaobabRemoteConnectDialogDetails {
	CtkWidget *grid;
	CtkWidget *type_combo;
	CtkWidget *uri_entry;
	CtkWidget *server_entry;
	CtkWidget *share_entry;
	CtkWidget *port_entry;
	CtkWidget *folder_entry;
	CtkWidget *domain_entry;
	CtkWidget *user_entry;
};

G_DEFINE_TYPE(BaobabRemoteConnectDialog, baobab_remote_connect_dialog, CTK_TYPE_DIALOG)

#define RESPONSE_CONNECT CTK_RESPONSE_OK


static void
display_error_dialog (GError *error,
		      GFile *location,
		      CtkWindow *parent)
{
	CtkWidget *dlg;
	char *parse_name;
	char *error_message;

	parse_name = g_file_get_parse_name (location);
	error_message = g_strdup_printf (_("Cannot scan location \"%s\""),
					 parse_name);
	g_free (parse_name);

	dlg = ctk_message_dialog_new (parent,
				      CTK_DIALOG_DESTROY_WITH_PARENT,
				      CTK_MESSAGE_ERROR,
				      CTK_BUTTONS_OK,
				      error_message, NULL);

	ctk_message_dialog_format_secondary_text (CTK_MESSAGE_DIALOG (dlg),
						  "%s", error->message);

	g_free (error_message);

	ctk_dialog_run (CTK_DIALOG (dlg));
	ctk_widget_destroy (dlg);
}

static void
mount_enclosing_ready_cb (GFile *location,
			  GAsyncResult *res,
			  CtkWindow *app)
{
	gboolean success;
	GError *error = NULL;

	success = g_file_mount_enclosing_volume_finish (location,
							res, &error);

	if (success ||
	    g_error_matches (error, G_IO_ERROR, G_IO_ERROR_ALREADY_MOUNTED)) {
		baobab_scan_location (location);
	}
	else {
		display_error_dialog (error, location, app);
	}

	if (error)
		g_error_free (error);

	if (location)
		g_object_unref (location);
}

static void
connect_server_dialog_present_uri (CtkWindow *app,
				   GFile *location,
				   CtkWidget *widget)
{
	GMountOperation *op;

	op = ctk_mount_operation_new (CTK_WINDOW (widget));

	g_file_mount_enclosing_volume (location,
				       0, op,
				       NULL,
				       (GAsyncReadyCallback) mount_enclosing_ready_cb,
				       app);
	g_object_unref (op);
}

struct MethodInfo {
	const char *scheme;
	guint flags;
};

/* A collection of flags for MethodInfo.flags */
enum {
	DEFAULT_METHOD = 0x00000001,

	/* Widgets to display in setup_for_type */
	SHOW_SHARE     = 0x00000010,
	SHOW_PORT      = 0x00000020,
	SHOW_USER      = 0x00000040,
	SHOW_DOMAIN    = 0x00000080,

	IS_ANONYMOUS   = 0x00001000
};

/* Remember to fill in descriptions below */
static struct MethodInfo methods[] = {
	/* FIXME: we need to alias ssh to sftp */
	{ "sftp",  SHOW_PORT | SHOW_USER },
	{ "ftp",  SHOW_PORT | SHOW_USER },
	{ "ftp",  DEFAULT_METHOD | IS_ANONYMOUS | SHOW_PORT},
	{ "smb",  SHOW_SHARE | SHOW_USER | SHOW_DOMAIN },
	{ "dav",  SHOW_PORT | SHOW_USER },
	/* FIXME: hrm, shouldn't it work? */
	{ "davs", SHOW_PORT | SHOW_USER },
	{ NULL,   0 }, /* Custom URI method */
};

/* To get around non constant gettext strings */
static const char*
get_method_description (struct MethodInfo *meth)
{
	if (!meth->scheme) {
		return _("Custom Location");
	} else if (strcmp (meth->scheme, "sftp") == 0) {
		return _("SSH");
	} else if (strcmp (meth->scheme, "ftp") == 0) {
		if (meth->flags & IS_ANONYMOUS) {
			return _("Public FTP");
		} else {
			return _("FTP (with login)");
		}
	} else if (strcmp (meth->scheme, "smb") == 0) {
		return _("Windows share");
	} else if (strcmp (meth->scheme, "dav") == 0) {
		return _("WebDAV (HTTP)");
	} else if (strcmp (meth->scheme, "davs") == 0) {
		return _("Secure WebDAV (HTTPS)");

	/* No descriptive text */
	} else {
		return meth->scheme;
	}
}

static void
baobab_remote_connect_dialog_finalize (GObject *object)
{
	BaobabRemoteConnectDialog *dialog;

	dialog = BAOBAB_REMOTE_CONNECT_DIALOG(object);

	g_object_unref (dialog->details->uri_entry);
	g_object_unref (dialog->details->server_entry);
	g_object_unref (dialog->details->share_entry);
	g_object_unref (dialog->details->port_entry);
	g_object_unref (dialog->details->folder_entry);
	g_object_unref (dialog->details->domain_entry);
	g_object_unref (dialog->details->user_entry);

	g_free (dialog->details);

	G_OBJECT_CLASS (baobab_remote_connect_dialog_parent_class)->finalize (object);
}

static void
connect_to_server (BaobabRemoteConnectDialog *dialog, CtkWindow *app)
{
	struct MethodInfo *meth;
	char *uri;
	GFile *location;
	int index;
	CtkTreeIter iter;

	/* Get our method info */
	ctk_combo_box_get_active_iter (CTK_COMBO_BOX (dialog->details->type_combo), &iter);
	ctk_tree_model_get (ctk_combo_box_get_model (CTK_COMBO_BOX (dialog->details->type_combo)),
			    &iter, 0, &index, -1);
	g_assert (index < G_N_ELEMENTS (methods) && index >= 0);
	meth = &(methods[index]);

	if (meth->scheme == NULL) {
		uri = ctk_editable_get_chars (CTK_EDITABLE (dialog->details->uri_entry), 0, -1);
		/* FIXME: we should validate it in some way? */
	} else {
		char *user, *port, *initial_path, *server, *folder ,*domain ;
		char *t, *join;
		gboolean free_initial_path, free_user, free_domain, free_port;

		server = ctk_editable_get_chars (CTK_EDITABLE (dialog->details->server_entry), 0, -1);
		if (strlen (server) == 0) {
			CtkWidget *dlg;

			dlg = ctk_message_dialog_new (CTK_WINDOW (dialog),
						      CTK_DIALOG_DESTROY_WITH_PARENT,
						      CTK_MESSAGE_ERROR,
						      CTK_BUTTONS_OK,
						      _("Cannot Connect to Server. You must enter a name for the server."));

			ctk_message_dialog_format_secondary_text (CTK_MESSAGE_DIALOG (dlg),
					_("Please enter a name and try again."));

			ctk_dialog_run (CTK_DIALOG (dlg));
    			ctk_widget_destroy (dlg);

			g_free (server);
			return;
		}

		user = "";
		port = "";
		initial_path = "";
		domain = "";
		free_initial_path = FALSE;
		free_user = FALSE;
		free_domain = FALSE;
		free_port = FALSE;

		/* FTP special case */
		if (meth->flags & IS_ANONYMOUS) {
			user = "anonymous";

		/* SMB special case */
		} else if (strcmp (meth->scheme, "smb") == 0) {
			t = ctk_editable_get_chars (CTK_EDITABLE (dialog->details->share_entry), 0, -1);
			initial_path = g_strconcat ("/", t, NULL);
			free_initial_path = TRUE;
			g_free (t);
		}

		if (ctk_widget_get_parent (dialog->details->port_entry) != NULL) {
			free_port = TRUE;
			port = ctk_editable_get_chars (CTK_EDITABLE (dialog->details->port_entry), 0, -1);
		}
		folder = ctk_editable_get_chars (CTK_EDITABLE (dialog->details->folder_entry), 0, -1);
		if (ctk_widget_get_parent (dialog->details->user_entry) != NULL) {
			free_user = TRUE;

			t = ctk_editable_get_chars (CTK_EDITABLE (dialog->details->user_entry), 0, -1);

			user = g_uri_escape_string (t, G_URI_RESERVED_CHARS_ALLOWED_IN_USERINFO, FALSE);

			g_free (t);
		}
		if (ctk_widget_get_parent (dialog->details->domain_entry) != NULL) {
			free_domain = TRUE;

			domain = ctk_editable_get_chars (CTK_EDITABLE (dialog->details->domain_entry), 0, -1);

			if (strlen (domain) != 0) {
				t = user;

				user = g_strconcat (domain , ";" , t, NULL);

				if (free_user) {
					g_free (t);
				}

				free_user = TRUE;
			}
		}

		if (folder[0] != 0 &&
		    folder[0] != '/') {
			join = "/";
		} else {
			join = "";
		}

		t = folder;
		folder = g_strconcat (initial_path, join, t, NULL);
		g_free (t);

		t = folder;
		folder = g_uri_escape_string (t, G_URI_RESERVED_CHARS_ALLOWED_IN_PATH, FALSE);
		g_free (t);

		uri = g_strdup_printf ("%s://%s%s%s%s%s%s",
				       meth->scheme,
				       user, (user[0] != 0) ? "@" : "",
				       server,
				       (port[0] != 0) ? ":" : "", port,
				       folder);

		if (free_initial_path) {
			g_free (initial_path);
		}
		g_free (server);
		if (free_port) {
			g_free (port);
		}
		g_free (folder);
		if (free_user) {
			g_free (user);
		}
		if (free_domain) {
			g_free (domain);
		}
	}

	ctk_widget_hide (CTK_WIDGET (dialog));

	location = g_file_new_for_uri (uri);
	g_free (uri);

	connect_server_dialog_present_uri (app,
					   location,
					   CTK_WIDGET (dialog));
}

static void
response_callback (BaobabRemoteConnectDialog *dialog,
		   int response_id,
		   CtkWindow *app)
{
	switch (response_id) {
	case RESPONSE_CONNECT:
		connect_to_server (dialog, app);
		break;
	case CTK_RESPONSE_NONE:
	case CTK_RESPONSE_DELETE_EVENT:
	case CTK_RESPONSE_CANCEL:
		break;
	default :
		g_assert_not_reached ();
	}
}

static void
baobab_remote_connect_dialog_class_init (BaobabRemoteConnectDialogClass *class)
{
	GObjectClass *gobject_class;

	gobject_class = G_OBJECT_CLASS (class);
	gobject_class->finalize = baobab_remote_connect_dialog_finalize;
}
static void
destroy_label (CtkWidget *widget,
               gpointer   user_data)
{
  ctk_widget_destroy (widget);
}

static void
setup_for_type (BaobabRemoteConnectDialog *dialog)
{
	struct MethodInfo *meth;
	int index, i;
	CtkWidget *label, *grid;
	CtkTreeIter iter;

	/* Get our method info */
	ctk_combo_box_get_active_iter (CTK_COMBO_BOX (dialog->details->type_combo), &iter);
	ctk_tree_model_get (ctk_combo_box_get_model (CTK_COMBO_BOX (dialog->details->type_combo)),
			    &iter, 0, &index, -1);
	g_assert (index < G_N_ELEMENTS (methods) && index >= 0);
	meth = &(methods[index]);

	if (ctk_widget_get_parent (dialog->details->uri_entry) != NULL) {
		ctk_container_remove (CTK_CONTAINER (dialog->details->grid),
				      dialog->details->uri_entry);
	}
	if (ctk_widget_get_parent (dialog->details->server_entry) != NULL) {
		ctk_container_remove (CTK_CONTAINER (dialog->details->grid),
				      dialog->details->server_entry);
	}
	if (ctk_widget_get_parent (dialog->details->share_entry) != NULL) {
		ctk_container_remove (CTK_CONTAINER (dialog->details->grid),
				      dialog->details->share_entry);
	}
	if (ctk_widget_get_parent (dialog->details->port_entry) != NULL) {
		ctk_container_remove (CTK_CONTAINER (dialog->details->grid),
				      dialog->details->port_entry);
	}
	if (ctk_widget_get_parent (dialog->details->folder_entry) != NULL) {
		ctk_container_remove (CTK_CONTAINER (dialog->details->grid),
				      dialog->details->folder_entry);
	}
	if (ctk_widget_get_parent (dialog->details->user_entry) != NULL) {
		ctk_container_remove (CTK_CONTAINER (dialog->details->grid),
				      dialog->details->user_entry);
	}
	if (ctk_widget_get_parent (dialog->details->domain_entry) != NULL) {
		ctk_container_remove (CTK_CONTAINER (dialog->details->grid),
				      dialog->details->domain_entry);
	}
	/* Destroy all labels */
	ctk_container_foreach (CTK_CONTAINER (dialog->details->grid),
			       destroy_label, NULL);

	i = 1;
	grid = dialog->details->grid;

	if (meth->scheme == NULL) {
		label = ctk_label_new_with_mnemonic (_("_Location (URI):"));
		ctk_label_set_xalign (CTK_LABEL (label), 0.0);
		ctk_label_set_yalign (CTK_LABEL (label), 0.5);
		ctk_widget_show (label);
		ctk_grid_attach (CTK_GRID (grid), label, 0, i, 1, 1);

		ctk_label_set_mnemonic_widget (CTK_LABEL (label), dialog->details->uri_entry);
		ctk_widget_set_hexpand (dialog->details->uri_entry, TRUE);
		ctk_widget_show (dialog->details->uri_entry);
		ctk_grid_attach (CTK_GRID (grid), dialog->details->uri_entry, 1, i, 1, 1);

		i++;

		return;
	}

	label = ctk_label_new_with_mnemonic (_("_Server:"));
	ctk_label_set_xalign (CTK_LABEL (label), 0.0);
	ctk_label_set_yalign (CTK_LABEL (label), 0.5);
	ctk_widget_show (label);
	ctk_grid_attach (CTK_GRID (grid), label, 0, i, 1, 1);

	ctk_label_set_mnemonic_widget (CTK_LABEL (label), dialog->details->server_entry);
	ctk_widget_set_hexpand (dialog->details->server_entry, TRUE);
	ctk_widget_show (dialog->details->server_entry);
	ctk_grid_attach (CTK_GRID (grid), dialog->details->server_entry, 1, i, 1, 1);

	i++;

	label = ctk_label_new (_("Optional information:"));
	ctk_label_set_xalign (CTK_LABEL (label), 0.0);
	ctk_label_set_yalign (CTK_LABEL (label), 0.5);
	ctk_widget_show (label);
	ctk_grid_attach (CTK_GRID (grid), label, 0, i, 2, 1);

	i++;

	if (meth->flags & SHOW_SHARE) {
		label = ctk_label_new_with_mnemonic (_("_Share:"));
		ctk_label_set_xalign (CTK_LABEL (label), 0.0);
		ctk_label_set_yalign (CTK_LABEL (label), 0.5);
		ctk_widget_show (label);
		ctk_grid_attach (CTK_GRID (grid), label, 0, i, 1, 1);

		ctk_label_set_mnemonic_widget (CTK_LABEL (label), dialog->details->share_entry);
		ctk_widget_set_hexpand (dialog->details->share_entry, TRUE);
		ctk_widget_show (dialog->details->share_entry);
		ctk_grid_attach (CTK_GRID (grid), dialog->details->share_entry, 1, i, 1, 1);

		i++;
	}

	if (meth->flags & SHOW_PORT) {
		label = ctk_label_new_with_mnemonic (_("_Port:"));
		ctk_label_set_xalign (CTK_LABEL (label), 0.0);
		ctk_label_set_yalign (CTK_LABEL (label), 0.5);
		ctk_widget_show (label);
		ctk_grid_attach (CTK_GRID (grid), label, 0, i, 1, 1);

		ctk_label_set_mnemonic_widget (CTK_LABEL (label), dialog->details->port_entry);
		ctk_widget_set_hexpand (dialog->details->port_entry, TRUE);
		ctk_widget_show (dialog->details->port_entry);
		ctk_grid_attach (CTK_GRID (grid), dialog->details->port_entry, 1, i, 1, 1);

		i++;
	}

	label = ctk_label_new_with_mnemonic (_("_Folder:"));
	ctk_label_set_xalign (CTK_LABEL (label), 0.0);
	ctk_label_set_yalign (CTK_LABEL (label), 0.5);
	ctk_widget_show (label);
	ctk_grid_attach (CTK_GRID (grid), label, 0, i, 1, 1);

	ctk_label_set_mnemonic_widget (CTK_LABEL (label), dialog->details->folder_entry);
	ctk_widget_set_hexpand (dialog->details->folder_entry, TRUE);
	ctk_widget_show (dialog->details->folder_entry);
	ctk_grid_attach (CTK_GRID (grid), dialog->details->folder_entry, 1, i, 1, 1);

	i++;

	if (meth->flags & SHOW_USER) {
		label = ctk_label_new_with_mnemonic (_("_User Name:"));
		ctk_label_set_xalign (CTK_LABEL (label), 0.0);
		ctk_label_set_yalign (CTK_LABEL (label), 0.5);
		ctk_widget_show (label);
		ctk_grid_attach (CTK_GRID (grid), label, 0, i, 1, 1);

		ctk_label_set_mnemonic_widget (CTK_LABEL (label), dialog->details->user_entry);
		ctk_widget_set_hexpand (dialog->details->user_entry, TRUE);
		ctk_widget_show (dialog->details->user_entry);
		ctk_grid_attach (CTK_GRID (grid), dialog->details->user_entry, 1, i, 1, 1);

		i++;
	}

	if (meth->flags & SHOW_DOMAIN) {
		label = ctk_label_new_with_mnemonic (_("_Domain Name:"));
		ctk_label_set_xalign (CTK_LABEL (label), 0.0);
		ctk_label_set_yalign (CTK_LABEL (label), 0.5);
		ctk_widget_show (label);
		ctk_grid_attach (CTK_GRID (grid), label, 0, i, 1, 1);

		ctk_label_set_mnemonic_widget (CTK_LABEL (label), dialog->details->domain_entry);
		ctk_widget_set_hexpand (dialog->details->user_entry, TRUE);
		ctk_widget_show (dialog->details->domain_entry);
		ctk_grid_attach (CTK_GRID (grid), dialog->details->domain_entry, 1, i, 1, 1);

                i++;
        }
}

static void
combo_changed_callback (CtkComboBox *combo_box,
			BaobabRemoteConnectDialog *dialog)
{
	setup_for_type (dialog);
}

static void
port_insert_text (CtkEditable *editable,
		  const gchar *new_text,
		  gint         new_text_length,
		  gint        *position)
{
	int pos;

	if (new_text_length < 0) {
		new_text_length = strlen (new_text);
	}

	/* Only allow digits to be inserted as port number */
	for (pos = 0; pos < new_text_length; pos++) {
		if (!g_ascii_isdigit (new_text[pos])) {
			CtkWidget *toplevel = ctk_widget_get_toplevel (CTK_WIDGET (editable));
			if (toplevel != NULL) {
				cdk_window_beep (ctk_widget_get_window (toplevel));
			}
		    g_signal_stop_emission_by_name (editable, "insert_text");
		    return;
		}
	}
}

static void
baobab_remote_connect_dialog_init (BaobabRemoteConnectDialog *dialog)
{
	CtkWidget *label;
	CtkWidget *grid;
	CtkWidget *combo;
	CtkWidget *hbox;
	CtkWidget *vbox;
	CtkListStore *store;
	CtkCellRenderer *renderer;
	int i;

	dialog->details = g_new0 (BaobabRemoteConnectDialogDetails, 1);

	ctk_window_set_title (CTK_WINDOW (dialog), _("Connect to Server"));
	ctk_container_set_border_width (CTK_CONTAINER (dialog), 5);
	ctk_box_set_spacing (CTK_BOX (ctk_dialog_get_content_area (CTK_DIALOG (dialog))), 2);
	ctk_window_set_resizable (CTK_WINDOW (dialog), FALSE);

	vbox = ctk_box_new (CTK_ORIENTATION_VERTICAL, 6);
	ctk_container_set_border_width (CTK_CONTAINER (vbox), 5);
	ctk_box_pack_start (CTK_BOX (ctk_dialog_get_content_area (CTK_DIALOG (dialog))),
			    vbox, FALSE, TRUE, 0);
	ctk_widget_show (vbox);

	hbox = ctk_box_new (CTK_ORIENTATION_HORIZONTAL, 6);
	ctk_box_pack_start (CTK_BOX (vbox),
			    hbox, FALSE, TRUE, 0);
	ctk_widget_show (hbox);

	label = ctk_label_new_with_mnemonic (_("Service _type:"));
	ctk_label_set_xalign (CTK_LABEL (label), 0.0);
	ctk_label_set_yalign (CTK_LABEL (label), 0.5);
	ctk_widget_show (label);
	ctk_box_pack_start (CTK_BOX (hbox),
			    label, FALSE, FALSE, 0);

	dialog->details->type_combo = combo = ctk_combo_box_new ();

	/* each row contains: method index, textual description */
	store = ctk_list_store_new (2, G_TYPE_INT, G_TYPE_STRING);
	ctk_combo_box_set_model (CTK_COMBO_BOX (combo), CTK_TREE_MODEL (store));
	g_object_unref (G_OBJECT (store));

	renderer = ctk_cell_renderer_text_new ();
	ctk_cell_layout_pack_start (CTK_CELL_LAYOUT (combo), renderer, TRUE);
	ctk_cell_layout_add_attribute (CTK_CELL_LAYOUT (combo), renderer, "text", 1);

	for (i = 0; i < G_N_ELEMENTS (methods); i++) {
		CtkTreeIter iter;
		const gchar * const *supported;
		int j;

		/* skip methods that don't have corresponding CafeVFSMethods */
		supported = g_vfs_get_supported_uri_schemes (g_vfs_get_default ());

		if (methods[i].scheme != NULL) {
			gboolean found;

			found = FALSE;
			for (j = 0; supported[j] != NULL; j++) {
				if (strcmp (methods[i].scheme, supported[j]) == 0) {
					found = TRUE;
					break;
				}
			}

			if (!found) {
				continue;
			}
		}

		ctk_list_store_append (store, &iter);
		ctk_list_store_set (store, &iter,
				    0, i,
				    1, get_method_description (&(methods[i])),
				    -1);


		if (methods[i].flags & DEFAULT_METHOD) {
			ctk_combo_box_set_active_iter (CTK_COMBO_BOX (combo), &iter);
		}
	}

	if (ctk_combo_box_get_active (CTK_COMBO_BOX (combo)) < 0) {
		/* default method not available, use any other */
		ctk_combo_box_set_active (CTK_COMBO_BOX (combo), 0);
	}

	ctk_widget_show (combo);
	ctk_label_set_mnemonic_widget (CTK_LABEL (label), combo);
	ctk_box_pack_start (CTK_BOX (hbox),
			    combo, TRUE, TRUE, 0);
	g_signal_connect (combo, "changed",
			  G_CALLBACK (combo_changed_callback),
			  dialog);

	hbox = ctk_box_new (CTK_ORIENTATION_HORIZONTAL, 6);
	ctk_box_pack_start (CTK_BOX (vbox),
			    hbox, FALSE, TRUE, 0);
	ctk_widget_show (hbox);

	label = ctk_label_new_with_mnemonic ("    ");
	ctk_widget_show (label);
	ctk_box_pack_start (CTK_BOX (hbox),
			    label, FALSE, FALSE, 0);


	dialog->details->grid = grid = ctk_grid_new ();
	ctk_grid_set_row_spacing (CTK_GRID (grid), 6);
	ctk_grid_set_column_spacing (CTK_GRID (grid), 12);
	ctk_widget_show (grid);
	ctk_box_pack_start (CTK_BOX (hbox),
			    grid, TRUE, TRUE, 0);

	dialog->details->uri_entry = ctk_entry_new();
	dialog->details->server_entry = ctk_entry_new ();
	dialog->details->share_entry = ctk_entry_new ();
	dialog->details->port_entry = ctk_entry_new ();
	g_signal_connect (dialog->details->port_entry, "insert_text", G_CALLBACK (port_insert_text),
			  NULL);
	dialog->details->folder_entry = ctk_entry_new ();
	dialog->details->domain_entry = ctk_entry_new ();
	dialog->details->user_entry = ctk_entry_new ();

	ctk_entry_set_activates_default (CTK_ENTRY (dialog->details->uri_entry), TRUE);
	ctk_entry_set_activates_default (CTK_ENTRY (dialog->details->server_entry), TRUE);
	ctk_entry_set_activates_default (CTK_ENTRY (dialog->details->share_entry), TRUE);
	ctk_entry_set_activates_default (CTK_ENTRY (dialog->details->port_entry), TRUE);
	ctk_entry_set_activates_default (CTK_ENTRY (dialog->details->folder_entry), TRUE);
	ctk_entry_set_activates_default (CTK_ENTRY (dialog->details->domain_entry), TRUE);
	ctk_entry_set_activates_default (CTK_ENTRY (dialog->details->user_entry), TRUE);

	/* We need an extra ref so we can remove them from the table */
	g_object_ref (dialog->details->uri_entry);
	g_object_ref (dialog->details->server_entry);
	g_object_ref (dialog->details->share_entry);
	g_object_ref (dialog->details->port_entry);
	g_object_ref (dialog->details->folder_entry);
	g_object_ref (dialog->details->domain_entry);
	g_object_ref (dialog->details->user_entry);

	setup_for_type (dialog);

	ctk_dialog_add_button (CTK_DIALOG (dialog),
			       CTK_STOCK_CANCEL,
			       CTK_RESPONSE_CANCEL);
	ctk_dialog_add_button (CTK_DIALOG (dialog),
			       _("_Scan"),
			       RESPONSE_CONNECT);
	ctk_dialog_set_default_response (CTK_DIALOG (dialog),
					 RESPONSE_CONNECT);
}

CtkWidget *
baobab_remote_connect_dialog_new (CtkWindow *window, GFile *location)
{
	CtkWidget *dialog;

	dialog = ctk_widget_new (BAOBAB_TYPE_REMOTE_CONNECT_DIALOG, NULL);

	ctk_window_set_transient_for (CTK_WINDOW (dialog), window);

	g_signal_connect (dialog, "response",
			  G_CALLBACK (response_callback),
			  window);

	if (window) {
		ctk_window_set_screen (CTK_WINDOW (dialog),
				       ctk_window_get_screen (CTK_WINDOW (window)));
	}

	return dialog;
}

