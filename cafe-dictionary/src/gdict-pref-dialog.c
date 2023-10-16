/* gdict-pref-dialog.c - preferences dialog
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
#include <time.h>
#include <sys/types.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <glib/gi18n.h>
#include <gio/gio.h>

#include "gdict-source-dialog.h"
#include "gdict-pref-dialog.h"
#include "gdict-common.h"

#define GDICT_PREFERENCES_UI 	PKGDATADIR "/cafe-dictionary-preferences.ui"

#define DEFAULT_MIN_WIDTH 	220
#define DEFAULT_MIN_HEIGHT 	330

/*******************
 * GdictPrefDialog *
 *******************/

static CtkWidget *global_dialog = NULL;

enum
{
  SOURCES_ACTIVE_COLUMN = 0,
  SOURCES_NAME_COLUMN,
  SOURCES_DESCRIPTION_COLUMN,

  SOURCES_N_COLUMNS
};

struct _GdictPrefDialog
{
  CtkDialog parent_instance;

  CtkBuilder *builder;

  GSettings *settings;

  gchar *active_source;
  GdictSourceLoader *loader;
  CtkListStore *sources_list;

  /* direct pointers to widgets */
  CtkWidget *notebook;

  CtkWidget *sources_view;
  CtkWidget *sources_add;
  CtkWidget *sources_remove;
  CtkWidget *sources_edit;

  gchar *print_font;
  CtkWidget *font_button;

  CtkWidget *help_button;
  CtkWidget *close_button;
};

struct _GdictPrefDialogClass
{
  CtkDialogClass parent_class;
};

enum
{
  PROP_0,

  PROP_SOURCE_LOADER
};


G_DEFINE_TYPE (GdictPrefDialog, gdict_pref_dialog, CTK_TYPE_DIALOG);


static gboolean
select_active_source_name (CtkTreeModel *model,
			   CtkTreePath  *path,
			   CtkTreeIter  *iter,
			   gpointer      data)
{
  GdictPrefDialog *dialog = GDICT_PREF_DIALOG (data);
  gboolean is_active;

  ctk_tree_model_get (model, iter,
      		      SOURCES_ACTIVE_COLUMN, &is_active,
      		      -1);
  if (is_active)
    {
      CtkTreeSelection *selection;

      selection = ctk_tree_view_get_selection (CTK_TREE_VIEW (dialog->sources_view));

      ctk_tree_selection_select_iter (selection, iter);

      return TRUE;
    }

  return FALSE;
}

static void
update_sources_view (GdictPrefDialog *dialog)
{
  const GSList *sources, *l;

  ctk_tree_view_set_model (CTK_TREE_VIEW (dialog->sources_view), NULL);

  ctk_list_store_clear (dialog->sources_list);

  /* force update of the sources list */
  gdict_source_loader_update (dialog->loader);

  sources = gdict_source_loader_get_sources (dialog->loader);
  for (l = sources; l != NULL; l = l->next)
    {
      GdictSource *source = GDICT_SOURCE (l->data);
      CtkTreeIter iter;
      const gchar *name, *description;
      gboolean is_active = FALSE;

      name = gdict_source_get_name (source);
      description = gdict_source_get_description (source);
      if (!description)
	description = name;

      if (strcmp (name, dialog->active_source) == 0)
        is_active = TRUE;

      ctk_list_store_append (dialog->sources_list, &iter);
      ctk_list_store_set (dialog->sources_list, &iter,
      			  SOURCES_ACTIVE_COLUMN, is_active,
      			  SOURCES_NAME_COLUMN, name,
      			  SOURCES_DESCRIPTION_COLUMN, description,
      			  -1);
    }

  ctk_tree_view_set_model (CTK_TREE_VIEW (dialog->sources_view),
  			   CTK_TREE_MODEL (dialog->sources_list));

  /* select the currently active source name */
  ctk_tree_model_foreach (CTK_TREE_MODEL (dialog->sources_list),
  			  select_active_source_name,
  			  dialog);
}

static void
source_renderer_toggled_cb (CtkCellRendererToggle *renderer,
			    const gchar           *path,
			    GdictPrefDialog       *dialog)
{
  CtkTreePath *treepath;
  CtkTreeIter iter;
  gboolean res;
  gboolean is_active;
  gchar *name;

  treepath = ctk_tree_path_new_from_string (path);
  res = ctk_tree_model_get_iter (CTK_TREE_MODEL (dialog->sources_list),
                                 &iter,
                                 treepath);
  if (!res)
    {
      ctk_tree_path_free (treepath);

      return;
    }

  ctk_tree_model_get (CTK_TREE_MODEL (dialog->sources_list), &iter,
      		      SOURCES_NAME_COLUMN, &name,
      		      SOURCES_ACTIVE_COLUMN, &is_active,
      		      -1);
  if (!is_active && name != NULL)
    {
      g_free (dialog->active_source);
      dialog->active_source = g_strdup (name);

      g_settings_set_string (dialog->settings, GDICT_SETTINGS_SOURCE_KEY, dialog->active_source);
      update_sources_view (dialog);

      g_free (name);
    }

  ctk_tree_path_free (treepath);
}

static void
sources_view_row_activated_cb (CtkTreeView       *tree_view,
			       CtkTreePath       *tree_path,
			       CtkTreeViewColumn *tree_iter,
			       GdictPrefDialog   *dialog)
{
  CtkWidget *edit_dialog;
  gchar *source_name;
  CtkTreeModel *model;
  CtkTreeIter iter;

  model = ctk_tree_view_get_model (tree_view);
  if (!model)
    return;

  if (!ctk_tree_model_get_iter (model, &iter, tree_path))
    return;

  ctk_tree_model_get (model, &iter, SOURCES_NAME_COLUMN, &source_name, -1);
  if (!source_name)
    return;

  edit_dialog = gdict_source_dialog_new (CTK_WINDOW (dialog),
  					 _("Edit Dictionary Source"),
  					 GDICT_SOURCE_DIALOG_EDIT,
  					 dialog->loader,
  					 source_name);
  ctk_dialog_run (CTK_DIALOG (edit_dialog));

  ctk_widget_destroy (edit_dialog);
  g_free (source_name);

  update_sources_view (dialog);
}

static void
build_sources_view (GdictPrefDialog *dialog)
{
  CtkTreeViewColumn *column;
  CtkCellRenderer *renderer;

  if (dialog->sources_list)
    return;

  dialog->sources_list = ctk_list_store_new (SOURCES_N_COLUMNS,
  					     G_TYPE_BOOLEAN,  /* active */
  					     G_TYPE_STRING,   /* name */
  					     G_TYPE_STRING    /* description */);
  ctk_tree_sortable_set_sort_column_id (CTK_TREE_SORTABLE (dialog->sources_list),
  					SOURCES_DESCRIPTION_COLUMN,
  					CTK_SORT_ASCENDING);

  renderer = ctk_cell_renderer_toggle_new ();
  ctk_cell_renderer_toggle_set_radio (CTK_CELL_RENDERER_TOGGLE (renderer), TRUE);
  g_signal_connect (renderer, "toggled",
  		    G_CALLBACK (source_renderer_toggled_cb),
  		    dialog);

  column = ctk_tree_view_column_new_with_attributes ("active",
  						     renderer,
  						     "active", SOURCES_ACTIVE_COLUMN,
  						     NULL);
  ctk_tree_view_append_column (CTK_TREE_VIEW (dialog->sources_view), column);

  renderer = ctk_cell_renderer_text_new ();
  column = ctk_tree_view_column_new_with_attributes ("description",
  						     renderer,
  						     "text", SOURCES_DESCRIPTION_COLUMN,
  						     NULL);
  ctk_tree_view_append_column (CTK_TREE_VIEW (dialog->sources_view), column);

  ctk_tree_view_set_headers_visible (CTK_TREE_VIEW (dialog->sources_view), FALSE);
  ctk_tree_view_set_model (CTK_TREE_VIEW (dialog->sources_view),
  			   CTK_TREE_MODEL (dialog->sources_list));

  g_signal_connect (dialog->sources_view, "row-activated",
		    G_CALLBACK (sources_view_row_activated_cb),
		    dialog);
}

static void
source_add_clicked_cb (CtkWidget       *widget,
		       GdictPrefDialog *dialog)
{
  CtkWidget *add_dialog;

  add_dialog = gdict_source_dialog_new (CTK_WINDOW (dialog),
  					_("Add Dictionary Source"),
  					GDICT_SOURCE_DIALOG_CREATE,
  					dialog->loader,
  					NULL);

  ctk_dialog_run (CTK_DIALOG (add_dialog));

  ctk_widget_destroy (add_dialog);

  update_sources_view (dialog);
}

static void
source_remove_clicked_cb (CtkWidget       *widget,
			  GdictPrefDialog *dialog)
{
  CtkTreeSelection *selection;
  CtkTreeModel *model;
  CtkTreeIter iter;
  gboolean is_selected;
  gchar *name, *description;

  selection = ctk_tree_view_get_selection (CTK_TREE_VIEW (dialog->sources_view));
  if (!selection)
    return;

  is_selected = ctk_tree_selection_get_selected (selection, &model, &iter);
  if (!is_selected)
    return;

  ctk_tree_model_get (model, &iter,
  		      SOURCES_NAME_COLUMN, &name,
  		      SOURCES_DESCRIPTION_COLUMN, &description,
  		      -1);
  if (!name)
    return;
  else
    {
      CtkWidget *confirm_dialog;
      gint response;

      confirm_dialog = ctk_message_dialog_new (CTK_WINDOW (dialog),
      					       CTK_DIALOG_DESTROY_WITH_PARENT,
      					       CTK_MESSAGE_WARNING,
      					       CTK_BUTTONS_NONE,
      					       _("Remove \"%s\"?"), description);
      ctk_message_dialog_format_secondary_text (CTK_MESSAGE_DIALOG (confirm_dialog),
      						_("This will permanently remove the "
      						  "dictionary source from the list."));

      ctk_dialog_add_button (CTK_DIALOG (confirm_dialog),
      			     "ctk-cancel",
      			     CTK_RESPONSE_CANCEL);
      ctk_dialog_add_button (CTK_DIALOG (confirm_dialog),
      			     "ctk-remove",
      			     CTK_RESPONSE_OK);

      ctk_window_set_title (CTK_WINDOW (confirm_dialog), "");

      response = ctk_dialog_run (CTK_DIALOG (confirm_dialog));
      if (response == CTK_RESPONSE_CANCEL)
        {
          ctk_widget_destroy (confirm_dialog);

          goto out;
        }

      ctk_widget_destroy (confirm_dialog);
    }

  if (gdict_source_loader_remove_source (dialog->loader, name))
    ctk_list_store_remove (CTK_LIST_STORE (model), &iter);
  else
    {
      CtkWidget *error_dialog;
      gchar *message;

      message = g_strdup_printf (_("Unable to remove source '%s'"),
      				 description);

      error_dialog = ctk_message_dialog_new (CTK_WINDOW (dialog),
      					     CTK_DIALOG_DESTROY_WITH_PARENT,
      					     CTK_MESSAGE_ERROR,
      					     CTK_BUTTONS_OK,
      					     "%s", message);
      ctk_window_set_title (CTK_WINDOW (error_dialog), "");

      ctk_dialog_run (CTK_DIALOG (error_dialog));

      ctk_widget_destroy (error_dialog);
    }

out:
  g_free (name);
  g_free (description);

  update_sources_view (dialog);
}

static void
source_edit_clicked_cb (CtkButton       *button,
                        GdictPrefDialog *dialog)
{
  CtkTreeSelection *selection;
  CtkTreeModel *model;
  CtkTreeIter iter;
  gboolean is_selected;
  gchar *name;

  selection = ctk_tree_view_get_selection (CTK_TREE_VIEW (dialog->sources_view));
  if (!selection)
    return;

  is_selected = ctk_tree_selection_get_selected (selection, &model, &iter);
  if (!is_selected)
    return;

  ctk_tree_model_get (model, &iter, SOURCES_NAME_COLUMN, &name, -1);
  if (!name)
    return;
  else
    {
      CtkWidget *edit_dialog;

      edit_dialog = gdict_source_dialog_new (CTK_WINDOW (dialog),
                                             _("Edit Dictionary Source"),
                                             GDICT_SOURCE_DIALOG_EDIT,
                                             dialog->loader,
                                             name);
      ctk_dialog_run (CTK_DIALOG (edit_dialog));

      ctk_widget_destroy (edit_dialog);
    }

  g_free (name);

  update_sources_view (dialog);
}

static void
set_source_loader (GdictPrefDialog   *dialog,
		   GdictSourceLoader *loader)
{
  if (!dialog->sources_list)
    return;

  if (dialog->loader)
    g_object_unref (dialog->loader);

  dialog->loader = g_object_ref (loader);

  update_sources_view (dialog);
}

static void
font_button_font_set_cb (CtkWidget       *font_button,
			 GdictPrefDialog *dialog)
{
  gchar *font;

  font = ctk_font_chooser_get_font (CTK_FONT_CHOOSER (font_button));

  if (!font || font[0] == '\0' || g_strcmp0 (dialog->print_font, font) == 0)
    {
      g_free (font);
      return;
    }

  g_free (dialog->print_font);
  dialog->print_font = g_strdup (font);

  g_settings_set_string (dialog->settings, GDICT_SETTINGS_PRINT_FONT_KEY, dialog->print_font);

  g_free (font);
}

static void
response_cb (CtkDialog *dialog,
	     gint       response_id,
	     gpointer   user_data)
{
  GError *err = NULL;

  switch (response_id)
    {
    case CTK_RESPONSE_HELP:
      ctk_show_uri_on_window (CTK_WINDOW (dialog),
                    "help:cafe-dictionary/cafe-dictionary-preferences",
                    ctk_get_current_event_time (), &err);
      if (err)
	{
          CtkWidget *error_dialog;
	  gchar *message;

	  message = g_strdup_printf (_("There was an error while displaying help"));
	  error_dialog = ctk_message_dialog_new (CTK_WINDOW (dialog),
      					         CTK_DIALOG_DESTROY_WITH_PARENT,
      					         CTK_MESSAGE_ERROR,
						 CTK_BUTTONS_OK,
						 "%s", message);
	  ctk_message_dialog_format_secondary_text (CTK_MESSAGE_DIALOG (error_dialog),
			  			    "%s", err->message);
	  ctk_window_set_title (CTK_WINDOW (error_dialog), "");

	  ctk_dialog_run (CTK_DIALOG (error_dialog));

          ctk_widget_destroy (error_dialog);
	  g_error_free (err);
        }

      /* we don't want the dialog to close itself */
      g_signal_stop_emission_by_name (dialog, "response");
      break;
    case CTK_RESPONSE_ACCEPT:
    default:
      ctk_widget_hide (CTK_WIDGET (dialog));
      break;
    }
}

static void
gdict_pref_dialog_finalize (GObject *object)
{
  GdictPrefDialog *dialog = GDICT_PREF_DIALOG (object);

  if (dialog->settings)
    g_object_unref (dialog->settings);

  if (dialog->builder)
    g_object_unref (dialog->builder);

  if (dialog->active_source)
    g_free (dialog->active_source);

  if (dialog->loader)
    g_object_unref (dialog->loader);

  G_OBJECT_CLASS (gdict_pref_dialog_parent_class)->finalize (object);
}

static void
gdict_pref_dialog_set_property (GObject      *object,
				guint         prop_id,
				const GValue *value,
				GParamSpec   *pspec)
{
  GdictPrefDialog *dialog = GDICT_PREF_DIALOG (object);

  switch (prop_id)
    {
    case PROP_SOURCE_LOADER:
      set_source_loader (dialog, g_value_get_object (value));
      break;
    default:
      break;
    }
}

static void
gdict_pref_dialog_get_property (GObject    *object,
				guint       prop_id,
				GValue     *value,
				GParamSpec *pspec)
{
  GdictPrefDialog *dialog = GDICT_PREF_DIALOG (object);

  switch (prop_id)
    {
    case PROP_SOURCE_LOADER:
      g_value_set_object (value, dialog->loader);
      break;
    default:
      break;
    }
}

static gboolean
gdict_dialog_page_scroll_event_cb (CtkWidget *widget, CdkEventScroll *event, CtkWindow *window)
{
  CtkNotebook *notebook = CTK_NOTEBOOK (widget);
  CtkWidget *child, *event_widget, *action_widget;

  child = ctk_notebook_get_nth_page (notebook, ctk_notebook_get_current_page (notebook));
  if (child == NULL)
      return FALSE;

  event_widget = ctk_get_event_widget ((CdkEvent *) event);

  /* Ignore scroll events from the content of the page */
  if (event_widget == NULL ||
      event_widget == child ||
      ctk_widget_is_ancestor (event_widget, child))
      return FALSE;

  /* And also from the action widgets */
  action_widget = ctk_notebook_get_action_widget (notebook, CTK_PACK_START);
  if (event_widget == action_widget ||
      (action_widget != NULL && ctk_widget_is_ancestor (event_widget, action_widget)))
      return FALSE;
  action_widget = ctk_notebook_get_action_widget (notebook, CTK_PACK_END);
  if (event_widget == action_widget ||
      (action_widget != NULL && ctk_widget_is_ancestor (event_widget, action_widget)))
      return FALSE;

  switch (event->direction) {
  case CDK_SCROLL_RIGHT:
  case CDK_SCROLL_DOWN:
    ctk_notebook_next_page (notebook);
    break;
  case CDK_SCROLL_LEFT:
  case CDK_SCROLL_UP:
    ctk_notebook_prev_page (notebook);
    break;
  case CDK_SCROLL_SMOOTH:
    switch (ctk_notebook_get_tab_pos (notebook)) {
    case CTK_POS_LEFT:
    case CTK_POS_RIGHT:
      if (event->delta_y > 0)
          ctk_notebook_next_page (notebook);
      else if (event->delta_y < 0)
          ctk_notebook_prev_page (notebook);
      break;
    case CTK_POS_TOP:
    case CTK_POS_BOTTOM:
      if (event->delta_x > 0)
          ctk_notebook_next_page (notebook);
      else if (event->delta_x < 0)
          ctk_notebook_prev_page (notebook);
      break;
    }
    break;
  }

  return TRUE;
}

static void
gdict_pref_dialog_class_init (GdictPrefDialogClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->set_property = gdict_pref_dialog_set_property;
  gobject_class->get_property = gdict_pref_dialog_get_property;
  gobject_class->finalize = gdict_pref_dialog_finalize;

  g_object_class_install_property (gobject_class,
  				   PROP_SOURCE_LOADER,
  				   g_param_spec_object ("source-loader",
  				   			"Source Loader",
  				   			"The GdictSourceLoader used by the application",
  				   			GDICT_TYPE_SOURCE_LOADER,
  				   			(G_PARAM_READABLE | G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY)));
}

static void
gdict_pref_dialog_init (GdictPrefDialog *dialog)
{
  gchar *font;
  GError *error = NULL;

  ctk_window_set_default_size (CTK_WINDOW (dialog),
  			       DEFAULT_MIN_WIDTH,
  			       DEFAULT_MIN_HEIGHT);

  ctk_container_set_border_width (CTK_CONTAINER (dialog), 5);
  ctk_box_set_spacing (CTK_BOX (ctk_dialog_get_content_area (CTK_DIALOG (dialog))), 2);

  /* add buttons */
  ctk_dialog_add_button (CTK_DIALOG (dialog),
  			 "ctk-help",
  			 CTK_RESPONSE_HELP);
  ctk_dialog_add_button (CTK_DIALOG (dialog),
  			 "ctk-close",
  			 CTK_RESPONSE_ACCEPT);

  dialog->settings = g_settings_new (GDICT_SETTINGS_SCHEMA);

  /* get the UI from the CtkBuilder file */
  dialog->builder = ctk_builder_new ();
  ctk_builder_add_from_file (dialog->builder, GDICT_PREFERENCES_UI, &error);

  if (error) {
    g_critical ("Unable to load the preferences user interface: %s", error->message);
    g_error_free (error);
    g_assert_not_reached ();
  }

  /* the main widget */
  ctk_container_add (CTK_CONTAINER (ctk_dialog_get_content_area (CTK_DIALOG (dialog))),
                     CTK_WIDGET (ctk_builder_get_object (dialog->builder, "preferences_root")));

  /* keep all the interesting widgets around */
  dialog->notebook = CTK_WIDGET (ctk_builder_get_object (dialog->builder, "preferences_notebook"));

  ctk_widget_add_events (dialog->notebook, CDK_SCROLL_MASK);
  g_signal_connect (dialog->notebook, "scroll-event",
                    G_CALLBACK (gdict_dialog_page_scroll_event_cb), CTK_WINDOW (dialog));

  dialog->sources_view = CTK_WIDGET (ctk_builder_get_object (dialog->builder, "sources_treeview"));
  build_sources_view (dialog);

  dialog->active_source = g_settings_get_string (dialog->settings, GDICT_SETTINGS_SOURCE_KEY);

  dialog->sources_add = CTK_WIDGET (ctk_builder_get_object (dialog->builder, "add_button"));
  ctk_widget_set_tooltip_text (dialog->sources_add,
                               _("Add a new dictionary source"));
  g_signal_connect (dialog->sources_add, "clicked",
  		    G_CALLBACK (source_add_clicked_cb), dialog);

  dialog->sources_remove = CTK_WIDGET (ctk_builder_get_object (dialog->builder, "remove_button"));
  ctk_widget_set_tooltip_text (dialog->sources_remove,
                               _("Remove the currently selected dictionary source"));
  g_signal_connect (dialog->sources_remove, "clicked",
  		    G_CALLBACK (source_remove_clicked_cb), dialog);

  dialog->sources_edit = CTK_WIDGET (ctk_builder_get_object (dialog->builder, "edit_button"));
  ctk_widget_set_tooltip_text (dialog->sources_edit,
                               _("Edit the currently selected dictionary source"));
  g_signal_connect (dialog->sources_edit, "clicked",
                    G_CALLBACK (source_edit_clicked_cb), dialog);

  font = g_settings_get_string (dialog->settings, GDICT_SETTINGS_PRINT_FONT_KEY);
  dialog->font_button = CTK_WIDGET (ctk_builder_get_object (dialog->builder, "print_font_button"));
  ctk_font_chooser_set_font (CTK_FONT_CHOOSER (dialog->font_button), font);
  ctk_widget_set_tooltip_text (dialog->font_button,
                               _("Set the font used for printing the definitions"));
  g_signal_connect (dialog->font_button, "font-set",
  		    G_CALLBACK (font_button_font_set_cb), dialog);
  g_free (font);

  ctk_widget_show_all (dialog->notebook);

  /* we want to intercept the response signal before any other
   * callbacks might be attached by the users of the
   * GdictPrefDialog widget.
   */
  g_signal_connect (dialog, "response",
		    G_CALLBACK (response_cb),
		    NULL);
}

void
gdict_show_pref_dialog (CtkWidget         *parent,
			const gchar       *title,
			GdictSourceLoader *loader)
{
  CtkWidget *dialog;

  g_return_if_fail (CTK_IS_WIDGET (parent));
  g_return_if_fail (GDICT_IS_SOURCE_LOADER (loader));

  if (parent != NULL)
    dialog = g_object_get_data (G_OBJECT (parent), "gdict-pref-dialog");
  else
    dialog = global_dialog;

  if (dialog == NULL)
    {
      dialog = g_object_new (GDICT_TYPE_PREF_DIALOG,
                             "source-loader", loader,
                             "title", title,
                             NULL);

      g_object_ref_sink (dialog);

      g_signal_connect (dialog, "delete-event",
                        G_CALLBACK (ctk_widget_hide_on_delete),
                        NULL);

      if (parent != NULL && CTK_IS_WINDOW (parent))
        {
          ctk_window_set_transient_for (CTK_WINDOW (dialog), CTK_WINDOW (parent));
          ctk_window_set_destroy_with_parent (CTK_WINDOW (dialog), TRUE);
          g_object_set_data_full (G_OBJECT (parent), "gdict-pref-dialog",
                                  dialog,
                                  g_object_unref);
        }
      else
        global_dialog = dialog;
    }

  ctk_window_set_screen (CTK_WINDOW (dialog), ctk_widget_get_screen (parent));
  ctk_window_present (CTK_WINDOW (dialog));
}
