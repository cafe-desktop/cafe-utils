/*
 * callbacks.c
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
 * Boston, MA  02110-1301  USA.
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <string.h>
#include <ctk/ctk.h>
#include <glib/gi18n.h>
#include <gio/gio.h>

#include "baobab.h"
#include "baobab-treeview.h"
#include "baobab-utils.h"
#include "callbacks.h"
#include "baobab-prefs.h"
#include "baobab-remote-connect-dialog.h"
#include "baobab-chart.h"

void
on_quit_activate (CtkMenuItem *menuitem, gpointer user_data)
{
	baobab_quit ();
}

void
on_menuscanhome_activate (CtkMenuItem *menuitem, gpointer user_data)
{
	baobab_scan_home ();
}

void
on_menuallfs_activate (CtkMenuItem *menuitem, gpointer user_data)
{
	baobab_scan_root ();
}

void
on_menuscandir_activate (CtkMenuItem *menuitem, gpointer user_data)
{
	dir_select (FALSE, baobab.window);
}

void
on_about_activate (CtkMenuItem *menuitem, gpointer user_data)
{
	const gchar * const authors[] = {
		"Fabio Marzocca <thesaltydog@gmail.com>",
		"Paolo Borelli <pborelli@katamail.com>",
		"Benoît Dejean <benoit@placenet.org>",
		"Igalia (rings-chart and treemap widget) <www.igalia.com>",
		NULL
	};

	const gchar* documenters[] = {
		"Fabio Marzocca <thesaltydog@gmail.com>",
		N_("CAFE Documentation Team"),
		NULL,
	};

	const gchar* license[] = {
		N_("This program is free software; you can redistribute it and/or "
		"modify it under the terms of the GNU General Public License as "
		"published by the Free Software Foundation; either version 2 of "
		"the License, or (at your option) any later version."),

		N_("This program is distributed in the hope that it will be useful, "
		"but WITHOUT ANY WARRANTY; without even the implied warranty of "
		"MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU "
		"General Public License for more details."),

		N_("You should have received a copy of the GNU General Public License "
		"along with this program; if not, write to the Free Software Foundation, Inc., 51 "
		"Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.")
	};


	gchar* license_trans = g_strjoin("\n\n", _(license[0]), _(license[1]), _(license[2]), NULL);

#ifdef ENABLE_NLS
	const char **p;
	for (p = documenters; *p; ++p)
		*p = _(*p);
#endif

	ctk_show_about_dialog (CTK_WINDOW (baobab.window),
		"program-name", _("Disk Usage Analyzer"),
		"version", VERSION,
		"title", _("About Disk Usage Analyzer"),
		"comments", _("A graphical tool to analyze disk usage."),
		"copyright", _("Copyright \xc2\xa9 2005-2010 Fabio Marzocca\n"
		               "Copyright \xc2\xa9 2011-2020 CAFE developers"),
		"logo-icon-name", "cafe-disk-usage-analyzer",
		"license", license_trans,
		"authors", authors,
		"documenters", documenters,
		"translator-credits", _("translator-credits"),
		"wrap-license", TRUE,
		NULL);

	g_free(license_trans);
}

void
on_menu_expand_activate (CtkMenuItem *menuitem, gpointer user_data)
{
	ctk_tree_view_expand_all (CTK_TREE_VIEW (baobab.tree_view));
}

void
on_menu_collapse_activate (CtkMenuItem *menuitem, gpointer user_data)
{
	ctk_tree_view_collapse_all (CTK_TREE_VIEW (baobab.tree_view));
}

void
on_menu_stop_activate (CtkMenuItem *menuitem, gpointer user_data)
{
	baobab_stop_scan ();
}

void
on_menu_rescan_activate (CtkMenuItem *menuitem, gpointer user_data)
{
	baobab_rescan_current_dir ();
}

void
on_tbscandir_clicked (CtkToolButton *toolbutton, gpointer user_data)
{
	dir_select (FALSE, baobab.window);
}

void
on_tbscanhome_clicked (CtkToolButton *toolbutton, gpointer user_data)
{
	baobab_scan_home ();
}

void
on_tbscanall_clicked (CtkToolButton *toolbutton, gpointer user_data)
{
	baobab_scan_root ();
}

void
on_tb_scan_remote_clicked (CtkToolButton *toolbutton, gpointer user_data)
{
	CtkWidget *dlg;

	dlg = baobab_remote_connect_dialog_new (CTK_WINDOW (baobab.window),
						NULL);
	ctk_dialog_run (CTK_DIALOG (dlg));

	ctk_widget_destroy (dlg);
}

void
on_menu_scan_rem_activate (CtkMenuItem *menuitem, gpointer user_data)
{
	on_tb_scan_remote_clicked (NULL, NULL);
}

void
on_tbstop_clicked (CtkToolButton *toolbutton, gpointer user_data)
{
	baobab_stop_scan ();
}

void
on_tbrescan_clicked (CtkToolButton *toolbutton, gpointer user_data)
{
	baobab_rescan_current_dir ();
}

gboolean
on_delete_activate (CtkWidget *widget,
		    CdkEvent *event, gpointer user_data)
{
	baobab_quit ();
	return TRUE;
}

void
open_file_cb (CtkMenuItem *pmenu, gpointer dummy)
{
	GFile *file;

	g_assert (!dummy);
	g_assert (baobab.selected_path);

	file = g_file_parse_name (baobab.selected_path);

	if (!g_file_query_exists (file, NULL)) {
		message (_("The document does not exist."), "",
		CTK_MESSAGE_INFO, baobab.window);
		g_object_unref (file);
		return;
	}

	open_file_with_application (file);
	g_object_unref (file);
}

void
trash_dir_cb (CtkMenuItem *pmenu, gpointer dummy)
{
	GFile *file;

	g_assert (!dummy);
	g_assert (baobab.selected_path);

	file = g_file_parse_name (baobab.selected_path);

	if (trash_file (file)) {
		CtkTreeIter iter;
		guint64 filesize;
		CtkTreeSelection *selection;

		selection =
		    ctk_tree_view_get_selection ((CtkTreeView *) baobab.
						 tree_view);
		ctk_tree_selection_get_selected (selection, NULL, &iter);
		ctk_tree_model_get ((CtkTreeModel *) baobab.model, &iter,
				    5, &filesize, -1);
		ctk_tree_store_remove (CTK_TREE_STORE (baobab.model),
				       &iter);
	}

	g_object_unref (file);
}

void
on_pref_menu (CtkAction *a, gpointer user_data)
{
	baobab_prefs_dialog ();
}

void
on_ck_allocated_activate (CtkToggleAction *action,
			  gpointer user_data)
{
	if (!baobab.is_local)
		return;

	baobab.show_allocated = ctk_toggle_action_get_active (action);

	baobab_treeview_show_allocated_size (baobab.tree_view,
					     baobab.show_allocated);

	baobab_set_busy (TRUE);
	baobab_set_statusbar (_("Calculating percentage bars..."));
	ctk_tree_model_foreach (CTK_TREE_MODEL (baobab.model),
				show_bars, NULL);
	baobab_set_busy (FALSE);
	baobab_set_statusbar (_("Ready"));
}

void
on_helpcontents_activate (CtkAction *a, gpointer user_data)
{
	baobab_help_display (CTK_WINDOW (baobab.window), "cafe-disk-usage-analyzer", NULL);
}

void
scan_folder_cb (CtkMenuItem *pmenu, gpointer dummy)
{
	GFile	*file;

	g_assert (!dummy);
	g_assert (baobab.selected_path);

	file = g_file_parse_name (baobab.selected_path);

	if (!g_file_query_exists (file, NULL)) {
		message (_("The folder does not exist."), "", CTK_MESSAGE_INFO, baobab.window);
	}

	baobab_scan_location (file);
	g_object_unref (file);
}

void
on_tv_selection_changed (CtkTreeSelection *selection, gpointer user_data)
{
	CtkTreeIter iter;

	if (ctk_tree_selection_get_selected (selection, NULL, &iter)) {
		CtkTreePath *path;

		path = ctk_tree_model_get_path (CTK_TREE_MODEL (baobab.model), &iter);

		baobab_chart_set_root (baobab.rings_chart, path);
		baobab_chart_set_root (baobab.treemap_chart, path);

		ctk_tree_path_free (path);
	}
}

void
on_move_upwards_cb (CtkCheckMenuItem *checkmenuitem, gpointer user_data)
{
	baobab_chart_move_up_root (baobab.current_chart);
}

void
on_zoom_in_cb (CtkCheckMenuItem *checkmenuitem, gpointer user_data)
{
	baobab_chart_zoom_in (baobab.current_chart);
}

void
on_zoom_out_cb (CtkCheckMenuItem *checkmenuitem, gpointer user_data)
{
	baobab_chart_zoom_out (baobab.current_chart);
}

void
on_chart_snapshot_cb (CtkCheckMenuItem *checkmenuitem, gpointer user_data)
{
	baobab_chart_save_snapshot (baobab.current_chart);
}

