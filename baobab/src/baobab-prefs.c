/*
 * baobab-prefs.c
 * This file is part of baobab
 *
 * Copyright (C) 2005-2006 Fabio Marzocca  <thesaltydog@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */


#include <config.h>

#include <string.h>
#include <sys/stat.h>
#include <ctk/ctk.h>
#include <glib/gi18n.h>
#include <glib/gstdio.h>
#include <glibtop/mountlist.h>
#include <glibtop/fsusage.h>
#include "baobab.h"
#include "baobab-utils.h"
#include "baobab-prefs.h"

#define BAOBAB_PREFERENCES_UI_RESOURCE "/org/cafe/disk-usage-analyzer/baobab-dialog-scan-props.ui"

enum
{
	COL_CHECK,
	COL_DEVICE,
	COL_MOUNT_D,
	COL_MOUNT,
	COL_TYPE,
	COL_FS_SIZE,
	COL_FS_AVAIL,
	TOT_COLUMNS
};

static gboolean
add_excluded_item (CtkTreeModel  *model,
		   CtkTreePath   *path,
		   CtkTreeIter   *iter,
		   GPtrArray     *uris)
{
	gchar *mount;
	gboolean check;

	ctk_tree_model_get (model,
			    iter,
			    COL_MOUNT, &mount,
			    COL_CHECK, &check,
			    -1);

	if (!check) {
		g_ptr_array_add (uris, mount);
	}
	else {
		g_free (mount);
	}

	return FALSE;
}

static gchar **
get_excluded_locations (CtkTreeModel *model)
{
	GPtrArray *uris;

	uris = g_ptr_array_new ();

	ctk_tree_model_foreach (model,
				(CtkTreeModelForeachFunc) add_excluded_item,
				uris);

	g_ptr_array_add (uris, NULL);

	return (gchar **) g_ptr_array_free (uris, FALSE);
}

static void
save_excluded_uris (CtkTreeModel *model)
{
	gchar **uris;

	uris = get_excluded_locations (model);

	g_settings_set_strv (baobab.prefs_settings,
			     BAOBAB_SETTINGS_EXCLUDED_URIS,
			     (const gchar * const *) uris);

	g_strfreev (uris);
}

static void
filechooser_response_cb (CtkDialog *dialog,
                         gint response_id,
                         CtkTreeModel *model)
{
	switch (response_id) {
		case CTK_RESPONSE_HELP:
			baobab_help_display (CTK_WINDOW (baobab.window),
			                     "cafe-disk-usage-analyzer", "baobab-preferences");
			break;
		case CTK_RESPONSE_DELETE_EVENT:
		case CTK_RESPONSE_CLOSE:
			save_excluded_uris (model);
		default:
			ctk_widget_destroy (CTK_WIDGET (dialog));
			break;
	}
}

static void
check_toggled (CtkCellRendererToggle *cell,
	       gchar *path_str,
	       CtkTreeModel *model)
{
	CtkTreeIter iter;
	CtkTreePath *path = ctk_tree_path_new_from_string (path_str);
	gboolean toggle;
	gchar *mountpoint;

	/* get toggled iter */
	ctk_tree_model_get_iter (model, &iter, path);
	ctk_tree_model_get (model,
			    &iter,
			    COL_CHECK, &toggle,
			    COL_MOUNT_D, &mountpoint,
			    -1);

	/* check if root dir */
	if (strcmp ("/", mountpoint) == 0)
		goto clean_up;

	/* set new value */
	ctk_list_store_set (CTK_LIST_STORE (model),
			    &iter,
			    COL_CHECK, !toggle,
			    -1);

 clean_up:
	g_free (mountpoint);
	ctk_tree_path_free (path);
}

static void
create_tree_props (CtkBuilder *builder, CtkTreeModel *model)
{
	CtkCellRenderer *cell;
	CtkTreeViewColumn *col;
	CtkWidget *tvw;

	tvw = CTK_WIDGET (ctk_builder_get_object (builder , "tree_view_props"));

	/* checkbox column */
	cell = ctk_cell_renderer_toggle_new ();
	g_signal_connect (cell, "toggled",
			  G_CALLBACK (check_toggled), model);

	col = ctk_tree_view_column_new_with_attributes (_("Scan"), cell,
							"active", COL_CHECK,
							NULL);
	ctk_tree_view_append_column (CTK_TREE_VIEW (tvw), col);

	/* First text column */
	cell = ctk_cell_renderer_text_new ();
	col = ctk_tree_view_column_new_with_attributes (_("Device"), cell,
							"markup", COL_DEVICE,
							"text", COL_DEVICE,
							NULL);
	ctk_tree_view_append_column (CTK_TREE_VIEW (tvw), col);

	/* second text column */
	cell = ctk_cell_renderer_text_new ();
	col = ctk_tree_view_column_new_with_attributes (_("Mount Point"),
							cell, "markup",
							COL_MOUNT_D, "text",
							COL_MOUNT_D, NULL);
	ctk_tree_view_append_column (CTK_TREE_VIEW (tvw), col);

	/* third text column */
	cell = ctk_cell_renderer_text_new ();
	col = ctk_tree_view_column_new_with_attributes (_("Filesystem Type"),
							cell, "markup",
							COL_TYPE, "text",
							COL_TYPE, NULL);
	ctk_tree_view_append_column (CTK_TREE_VIEW (tvw), col);

	/* fourth text column */
	cell = ctk_cell_renderer_text_new ();
	col = ctk_tree_view_column_new_with_attributes (_("Total Size"),
							cell, "markup",
							COL_FS_SIZE, "text",
							COL_FS_SIZE, NULL);
	g_object_set (G_OBJECT (cell), "xalign", (gfloat) 1.0, NULL);
	ctk_tree_view_append_column (CTK_TREE_VIEW (tvw), col);

	/* fifth text column */
	cell = ctk_cell_renderer_text_new ();
	col = ctk_tree_view_column_new_with_attributes (_("Available"),
							cell, "markup",
							COL_FS_AVAIL, "text",
							COL_FS_AVAIL, NULL);
	g_object_set (G_OBJECT (cell), "xalign", (gfloat) 1.0, NULL);
	ctk_tree_view_append_column (CTK_TREE_VIEW (tvw), col);

	ctk_tree_view_set_model (CTK_TREE_VIEW (tvw), model);
	g_object_unref (model);
}

static void
fill_props_model (CtkListStore *store)
{
	CtkTreeIter iter;
	guint lo;
	glibtop_mountlist mountlist;
	glibtop_mountentry *mountentry, *mountentry_tofree;
	guint64 fstotal, fsavail;

	mountentry_tofree = glibtop_get_mountlist (&mountlist, 0);

	for (lo = 0, mountentry = mountentry_tofree;
	     lo < mountlist.number;
	     lo++, mountentry++) {
		glibtop_fsusage fsusage;
		gchar * total, *avail;
		GFile *file;
		gchar *uri;
		gboolean excluded;

		struct stat buf;
		if (g_stat (mountentry->devname,&buf) == -1)
			continue;

		glibtop_get_fsusage (&fsusage, mountentry->mountdir);
		fstotal = fsusage.blocks * fsusage.block_size;
		fsavail = fsusage.bfree * fsusage.block_size;

			total = g_format_size(fstotal);
			avail = g_format_size(fsavail);

		file = g_file_new_for_path (mountentry->mountdir);
		uri = g_file_get_uri (file);
		excluded = baobab_is_excluded_location (file);

		ctk_list_store_append (store, &iter);
		ctk_list_store_set (store, &iter,
				    COL_CHECK, !excluded,
				    COL_DEVICE, mountentry->devname,
				    COL_MOUNT_D, mountentry->mountdir,
				    COL_MOUNT, uri,
				    COL_TYPE, mountentry->type,
				    COL_FS_SIZE, total,
				    COL_FS_AVAIL, avail,
				    -1);
		g_free(total);
		g_free(avail);
		g_free(uri);
		g_object_unref(file);
	}

	g_free (mountentry_tofree);
}

void
baobab_prefs_dialog (void)
{
	CtkBuilder *builder;
	CtkWidget *dlg;
	CtkWidget *check_enablehome;
	CtkListStore *model;
	GError *error = NULL;

	builder = ctk_builder_new ();

	if (ctk_builder_add_from_resource (builder, BAOBAB_PREFERENCES_UI_RESOURCE, &error) == 0) {
		g_critical ("Can't load user interface resource for the scan properties dialog: %s",
			    error->message);
		g_object_unref (builder);
		g_error_free (error);

		return;
	}

	dlg = CTK_WIDGET (ctk_builder_get_object (builder, "dialog_scan_props"));

	ctk_window_set_transient_for (CTK_WINDOW (dlg),
				      CTK_WINDOW (baobab.window));

	model = ctk_list_store_new (TOT_COLUMNS,
				    G_TYPE_BOOLEAN,	/* checkbox */
				    G_TYPE_STRING,	/* device */
				    G_TYPE_STRING,	/*mount point display */
				    G_TYPE_STRING,	/* mount point uri */
				    G_TYPE_STRING,	/* fs type */
				    G_TYPE_STRING,	/* fs size */
				    G_TYPE_STRING	/* fs avail */
				    );

	create_tree_props (builder, CTK_TREE_MODEL (model));
	fill_props_model (model);

	check_enablehome = CTK_WIDGET (ctk_builder_get_object (builder, "check_enable_home"));
	g_settings_bind (baobab.prefs_settings,
			 BAOBAB_SETTINGS_MONITOR_HOME,
			 check_enablehome, "active",
			 G_SETTINGS_BIND_DEFAULT);

	g_signal_connect (dlg, "response",
			  G_CALLBACK (filechooser_response_cb), model);

	ctk_widget_show_all (dlg);
}

