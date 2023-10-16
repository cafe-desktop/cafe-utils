/*-*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/*
 * cafe-utils
 * Copyright (C) Johannes Schmid 2009 <jhs@gnome.org>
 *
 * cafe-utils is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * cafe-utils is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "logview-filter-manager.h"
#include "logview-prefs.h"
#include <ctk/ctk.h>
#include <string.h>
#include <glib/gi18n.h>

#define UI_RESOURCE "/org/cafe/system-log/logview-filter.ui"

struct _LogviewFilterManagerPrivate {
  GtkWidget *tree;

  GtkWidget *add_button;
  GtkWidget *remove_button;
  GtkWidget *edit_button;

  GtkTreeModel *model;
  GtkBuilder* builder;

  LogviewPrefs* prefs;
};

enum {
  COLUMN_NAME = 0,
  COLUMN_FILTER,
  N_COLUMNS
};

G_DEFINE_TYPE_WITH_PRIVATE (LogviewFilterManager, logview_filter_manager, CTK_TYPE_DIALOG);

static void
logview_filter_manager_update_model (LogviewFilterManager *manager)
{
  GList *filters;
  GList *filter;
  gchar *name;
  GtkTreeIter iter;

  ctk_list_store_clear (CTK_LIST_STORE (manager->priv->model));

  filters = logview_prefs_get_filters (manager->priv->prefs);

  for (filter = filters; filter != NULL; filter = g_list_next (filter)) {
    g_object_get (filter->data, "name", &name, NULL);

    ctk_list_store_append (CTK_LIST_STORE(manager->priv->model), &iter);
    ctk_list_store_set (CTK_LIST_STORE (manager->priv->model),
                        &iter,
                        COLUMN_NAME, name,
                        COLUMN_FILTER, filter->data,
                        -1);

    g_free (name);
  }

  g_list_free (filters);
}

static gboolean
check_name (LogviewFilterManager *manager, const gchar *name)
{
  GtkWidget *dialog;

  if (!strlen (name)) {
    dialog = ctk_message_dialog_new (CTK_WINDOW (manager),
                                     CTK_DIALOG_MODAL,
                                     CTK_MESSAGE_ERROR,
                                     CTK_BUTTONS_CLOSE,
                                     "%s", _("Filter name is empty!"));
    ctk_dialog_run (CTK_DIALOG (dialog));

    ctk_widget_destroy (dialog);

    return FALSE;
  }

  if (strstr (name, ":") != NULL) {
    dialog = ctk_message_dialog_new (CTK_WINDOW(manager),
                                     CTK_DIALOG_MODAL,
                                     CTK_MESSAGE_ERROR,
                                     CTK_BUTTONS_CLOSE,
                                     "%s", _("Filter name may not contain the ':' character"));
    ctk_dialog_run (CTK_DIALOG (dialog));

    ctk_widget_destroy (dialog);

    return FALSE;
  }

  return TRUE;
}

static gboolean
check_regex (LogviewFilterManager *manager, const gchar *regex)
{
  GtkWidget *dialog;
  GError *error = NULL;
  GRegex *reg;

  if (!strlen (regex)) {
    dialog = ctk_message_dialog_new (CTK_WINDOW(manager),
                                     CTK_DIALOG_MODAL,
                                     CTK_MESSAGE_ERROR,
                                     CTK_BUTTONS_CLOSE,
                                     "%s", _("Regular expression is empty!"));

    ctk_dialog_run (CTK_DIALOG (dialog));

    ctk_widget_destroy (dialog);

    return FALSE;
  }

  reg = g_regex_new (regex,
                     0, 0, &error);
  if (error) {
    dialog = ctk_message_dialog_new (CTK_WINDOW (manager),
                                     CTK_DIALOG_MODAL,
                                     CTK_MESSAGE_ERROR,
                                     CTK_BUTTONS_CLOSE,
                                     _("Regular expression is invalid: %s"),
                                     error->message);
    ctk_dialog_run (CTK_DIALOG (dialog));

    ctk_widget_destroy (dialog);
    g_error_free (error);

    return FALSE;
  }

  g_regex_unref (reg);

  return TRUE;
}

static void
on_check_toggled (GtkToggleButton *button, GtkWidget *widget)
{
  ctk_widget_set_sensitive (widget,
                            ctk_toggle_button_get_active (button));
}

static void
on_dialog_add_edit_reponse (GtkWidget *dialog, int response_id,
                            LogviewFilterManager *manager)
{
  GtkWidget *entry_name, *entry_regex;
  GtkWidget *radio_color;
  GtkWidget *check_foreground, *check_background;
  GtkWidget *color_foreground, *color_background;
  gchar *old_name;
  const gchar *name;
  const gchar *regex;
  LogviewFilter *filter;
  GtkTextTag *tag;
  GtkBuilder *builder;

  old_name = g_object_get_data (G_OBJECT (manager), "old_name");
  builder = manager->priv->builder;

  entry_name = CTK_WIDGET (ctk_builder_get_object (builder,
                                                   "entry_name"));
  entry_regex = CTK_WIDGET (ctk_builder_get_object (builder,
                                                    "entry_regex"));
  radio_color = CTK_WIDGET (ctk_builder_get_object (builder,
                                                    "radio_color"));
  check_foreground = CTK_WIDGET (ctk_builder_get_object (builder,
                                                         "check_foreground"));
  check_background = CTK_WIDGET (ctk_builder_get_object (builder,
                                                         "check_background"));
  color_foreground = CTK_WIDGET (ctk_builder_get_object (builder,
                                                         "color_foreground"));
  color_background = CTK_WIDGET (ctk_builder_get_object (builder,
                                                         "color_background"));

  if (response_id == CTK_RESPONSE_APPLY) {
    name = ctk_entry_get_text (CTK_ENTRY (entry_name));
    regex = ctk_entry_get_text (CTK_ENTRY (entry_regex));

    if (!check_name (manager, name) || !check_regex (manager, regex)) {
      return;
    }

    filter = logview_filter_new (name, regex);
    tag = ctk_text_tag_new (name);

    if (ctk_toggle_button_get_active (CTK_TOGGLE_BUTTON (radio_color))) {
      if (ctk_toggle_button_get_active (CTK_TOGGLE_BUTTON (check_foreground))) {
        GdkRGBA foreground_color;
        ctk_color_chooser_get_rgba (CTK_COLOR_CHOOSER (color_foreground),
                                    &foreground_color);
        g_object_set (G_OBJECT (tag),
                      "foreground-rgba", &foreground_color,
                      "foreground-set", TRUE, NULL);
      }

      if (ctk_toggle_button_get_active (CTK_TOGGLE_BUTTON (check_background))) {
        GdkRGBA background_color;
        ctk_color_chooser_get_rgba (CTK_COLOR_CHOOSER (color_background),
                                    &background_color);
        g_object_set (tag,
                      "paragraph-background-rgba", &background_color,
                      "paragraph-background-set", TRUE, NULL);
      }

      if (!ctk_toggle_button_get_active (CTK_TOGGLE_BUTTON (check_foreground))
          && !ctk_toggle_button_get_active (CTK_TOGGLE_BUTTON (check_background))) {
          GtkWidget *error_dialog;

          error_dialog = ctk_message_dialog_new (CTK_WINDOW (manager),
                                                 CTK_DIALOG_MODAL,
                                                 CTK_MESSAGE_ERROR,
                                                 CTK_BUTTONS_CLOSE,
                                                 "%s",
                                                 _("Please specify either foreground or background color!"));
          ctk_dialog_run (CTK_DIALOG (error_dialog));
          ctk_widget_destroy (error_dialog);
          g_object_unref (tag);
          g_object_unref (filter);

          return;
      }
    } else { /* !ctk_toggle_button_get_active (CTK_TOGGLE_BUTTON (radio_color)) */
      g_object_set (tag, "invisible", TRUE, NULL);
    }

    if (old_name && !g_str_equal (old_name, name)) {
      logview_prefs_remove_filter (manager->priv->prefs, old_name);
    }

    g_object_set (G_OBJECT (filter), "texttag", tag, NULL);
    g_object_unref (tag);

    logview_prefs_add_filter (manager->priv->prefs, filter);
    g_object_unref (filter);

    logview_filter_manager_update_model (manager);
  }

  ctk_widget_destroy (dialog);
}

static void
run_add_edit_dialog (LogviewFilterManager *manager, LogviewFilter *filter)
{
  GError *error;
  gchar *name, *regex;
  const gchar *title;
  GtkWidget *dialog, *entry_name, *entry_regex, *radio_color;
  GtkWidget *radio_visible, *check_foreground, *check_background;
  GtkWidget *color_foreground, *color_background, *vbox_color;
  gboolean foreground_set, background_set, invisible;
  GtkTextTag *tag;
  GtkBuilder* builder;

  builder = manager->priv->builder;

  error = NULL;
  name = NULL;

  if (ctk_builder_add_from_resource (builder, UI_RESOURCE, &error) == 0) {
    g_warning ("Could not load filter ui: %s", error->message);
    g_error_free (error);
    return;
  }

  title = (filter != NULL ? _("Edit filter") : _("Add new filter"));

  dialog = CTK_WIDGET (ctk_builder_get_object (builder,
                                               "dialog_filter"));

  ctk_window_set_title (CTK_WINDOW (dialog), title);

  entry_name = CTK_WIDGET (ctk_builder_get_object (builder,
                                                   "entry_name"));
  entry_regex = CTK_WIDGET (ctk_builder_get_object (builder,
                                                    "entry_regex"));
  radio_color = CTK_WIDGET (ctk_builder_get_object (builder,
                                                    "radio_color"));
  radio_visible = CTK_WIDGET (ctk_builder_get_object (builder,
                                                      "radio_visible"));

  ctk_radio_button_set_group (CTK_RADIO_BUTTON (radio_color),
                              ctk_radio_button_get_group (CTK_RADIO_BUTTON (radio_visible)));

  check_foreground = CTK_WIDGET (ctk_builder_get_object (builder,
                                                         "check_foreground"));
  check_background = CTK_WIDGET (ctk_builder_get_object (builder,
                                                         "check_background"));
  color_foreground = CTK_WIDGET (ctk_builder_get_object (builder,
                                                         "color_foreground"));
  color_background = CTK_WIDGET (ctk_builder_get_object (builder,
                                                         "color_background"));
  g_signal_connect (check_foreground, "toggled", G_CALLBACK (on_check_toggled),
                    color_foreground);
  g_signal_connect (check_background, "toggled", G_CALLBACK (on_check_toggled),
                    color_background);

  on_check_toggled (CTK_TOGGLE_BUTTON (check_foreground),
                    color_foreground);
  on_check_toggled (CTK_TOGGLE_BUTTON (check_background),
                    color_background);

  vbox_color = CTK_WIDGET (ctk_builder_get_object (builder, "vbox_color"));
  g_signal_connect (radio_color, "toggled", G_CALLBACK (on_check_toggled),
                    vbox_color);
  on_check_toggled (CTK_TOGGLE_BUTTON (radio_color),
                    vbox_color);

  if (filter) {
    g_object_get (filter,
                  "name", &name,
                  "regex", &regex,
                  "texttag", &tag,
                  NULL);
    g_object_get (tag,
                  "foreground-set", &foreground_set,
                  "paragraph-background-set", &background_set,
                  "invisible", &invisible, NULL);
    ctk_entry_set_text (CTK_ENTRY(entry_name), name);
    ctk_entry_set_text (CTK_ENTRY(entry_regex), regex);

    if (foreground_set) {
      GdkRGBA *foreground;

      g_object_get (tag, "foreground-rgba", &foreground, NULL);
      ctk_color_chooser_set_rgba (CTK_COLOR_CHOOSER (color_foreground),
                                  foreground);
      ctk_toggle_button_set_active (CTK_TOGGLE_BUTTON (check_foreground),
                                    TRUE);

      gdk_rgba_free (foreground);
    }

    if (background_set) {
      GdkRGBA *background;

      g_object_get (tag, "paragraph-background-rgba", &background, NULL);
      ctk_color_chooser_set_rgba (CTK_COLOR_CHOOSER (color_background),
                                  background);
      ctk_toggle_button_set_active (CTK_TOGGLE_BUTTON (check_background),
                                    TRUE);

      gdk_rgba_free (background);
    }

    if (background_set || foreground_set) {
      ctk_toggle_button_set_active (CTK_TOGGLE_BUTTON (radio_color), TRUE);
    } else if (invisible) {
      ctk_toggle_button_set_active (CTK_TOGGLE_BUTTON (radio_visible), TRUE);
    }

    g_free (regex);
    g_object_unref (tag);
  }

  g_object_set_data_full (G_OBJECT (manager), "old_name", name, g_free);

  g_signal_connect (G_OBJECT (dialog), "response",
                    G_CALLBACK (on_dialog_add_edit_reponse), manager);
  ctk_window_set_transient_for (CTK_WINDOW (dialog),
                                CTK_WINDOW (manager));
  ctk_window_set_modal (CTK_WINDOW (dialog),
                        TRUE);

  ctk_widget_show (CTK_WIDGET (dialog));
}

static void
on_add_clicked (GtkWidget *button, LogviewFilterManager *manager)
{
  run_add_edit_dialog (manager, NULL);
}

static void
on_edit_clicked (GtkWidget *button, LogviewFilterManager *manager)
{
  GtkTreeIter iter;
  GtkTreeModel *model;
  LogviewFilter *filter;
  GtkTreeSelection *selection;

  selection =
    ctk_tree_view_get_selection (CTK_TREE_VIEW (manager->priv->tree));

  ctk_tree_selection_get_selected (selection, &model, &iter);
  ctk_tree_model_get (model, &iter, COLUMN_FILTER, &filter, -1);

  run_add_edit_dialog (manager, filter);

  g_object_unref (filter);
}

static void
on_remove_clicked (GtkWidget *button, LogviewFilterManager *manager)
{
  GtkTreeSelection *selection;
  GtkTreeIter iter;
  GtkTreeModel *model;
  gchar *name;

  selection  =
    ctk_tree_view_get_selection (CTK_TREE_VIEW (manager->priv->tree));

  ctk_tree_selection_get_selected (selection, &model, &iter);
  ctk_tree_model_get (model, &iter, COLUMN_NAME, &name, -1);

  logview_prefs_remove_filter (manager->priv->prefs, name);
  logview_filter_manager_update_model (manager);

  g_free(name);
}

static void
on_tree_selection_changed (GtkTreeSelection *selection, LogviewFilterManager *manager)
{
  gboolean status;

  status = ctk_tree_selection_get_selected (selection, NULL, NULL);

  ctk_widget_set_sensitive (manager->priv->edit_button, status);
  ctk_widget_set_sensitive (manager->priv->remove_button, status);
}

static void
logview_filter_manager_init (LogviewFilterManager *manager)
{
  GtkWidget *grid;
  GtkWidget *scrolled_window;
  GtkTreeViewColumn *column;
  GtkCellRenderer *text_renderer;
  LogviewFilterManagerPrivate *priv;

  manager->priv = logview_filter_manager_get_instance_private (manager);
  priv = manager->priv;

  priv->builder = ctk_builder_new ();
  g_object_ref (priv->builder);
  priv->prefs = logview_prefs_get ();

  ctk_dialog_add_button (CTK_DIALOG(manager),
                         "ctk-close",
                         CTK_RESPONSE_CLOSE);
  ctk_window_set_modal (CTK_WINDOW (manager),
                        TRUE);

  priv->model = CTK_TREE_MODEL (ctk_list_store_new (N_COLUMNS,
                                                    G_TYPE_STRING,
                                                    G_TYPE_OBJECT));
  logview_filter_manager_update_model (manager);

  grid = ctk_grid_new ();
  ctk_grid_set_row_spacing (CTK_GRID (grid), 6);
  ctk_grid_set_column_spacing (CTK_GRID (grid), 6);
  scrolled_window = ctk_scrolled_window_new (NULL, NULL);
  ctk_scrolled_window_set_policy (CTK_SCROLLED_WINDOW (scrolled_window),
                                  CTK_POLICY_AUTOMATIC, CTK_POLICY_AUTOMATIC);
  ctk_scrolled_window_set_shadow_type (CTK_SCROLLED_WINDOW (scrolled_window),
                                       CTK_SHADOW_ETCHED_IN);
  priv->tree = ctk_tree_view_new_with_model (priv->model);
  ctk_widget_set_size_request (priv->tree, 150, 200);
  ctk_tree_view_set_headers_visible (CTK_TREE_VIEW (priv->tree), FALSE);
  ctk_container_add (CTK_CONTAINER (scrolled_window), priv->tree);

  text_renderer = ctk_cell_renderer_text_new ();

  column = ctk_tree_view_column_new();
  ctk_tree_view_column_pack_start (column, text_renderer, TRUE);
  ctk_tree_view_column_set_attributes (column,
                                       text_renderer,
                                       "text", COLUMN_NAME,
                                       NULL);
  ctk_tree_view_append_column (CTK_TREE_VIEW (priv->tree),
                               column);

  priv->add_button = CTK_WIDGET (g_object_new (CTK_TYPE_BUTTON,
					       "label", "ctk-add",
					       "use-stock", TRUE,
					       "use-underline", TRUE,
					       NULL));

  priv->edit_button = CTK_WIDGET (g_object_new (CTK_TYPE_BUTTON,
						"label", "ctk-properties",
						"use-stock", TRUE,
						"use-underline", TRUE,
						NULL));

  priv->remove_button = CTK_WIDGET (g_object_new (CTK_TYPE_BUTTON,
						  "label", "ctk-remove",
						  "use-stock", TRUE,
						  "use-underline", TRUE,
						  NULL));

  ctk_window_set_title (CTK_WINDOW (manager),
                        _("Filters"));

  g_signal_connect (priv->add_button, "clicked",
                    G_CALLBACK (on_add_clicked), manager);
  g_signal_connect (priv->edit_button, "clicked",
                    G_CALLBACK (on_edit_clicked), manager);
  g_signal_connect (priv->remove_button, "clicked",
                    G_CALLBACK (on_remove_clicked), manager);

  ctk_widget_set_sensitive (priv->edit_button, FALSE);
  ctk_widget_set_sensitive (priv->remove_button, FALSE);

  g_signal_connect (ctk_tree_view_get_selection (CTK_TREE_VIEW (priv->tree)),
                    "changed", G_CALLBACK (on_tree_selection_changed),
                    manager);

  ctk_widget_set_hexpand (scrolled_window, TRUE);
  ctk_widget_set_vexpand (scrolled_window, TRUE);
  ctk_grid_attach (CTK_GRID (grid), scrolled_window, 0, 0, 1, 3);
  ctk_widget_set_valign (priv->add_button, CTK_ALIGN_CENTER);
  ctk_grid_attach (CTK_GRID (grid), priv->add_button, 1, 0, 1, 1);
  ctk_widget_set_valign (priv->edit_button, CTK_ALIGN_CENTER);
  ctk_grid_attach (CTK_GRID (grid), priv->edit_button, 1, 1, 1, 1);
  ctk_widget_set_valign (priv->remove_button, CTK_ALIGN_CENTER);
  ctk_grid_attach (CTK_GRID (grid), priv->remove_button, 1, 2, 1, 1);
  ctk_box_pack_start (CTK_BOX (ctk_dialog_get_content_area (CTK_DIALOG (manager))),
                      grid, TRUE, TRUE, 5);
  ctk_widget_show_all (CTK_WIDGET (manager));
}

static void
logview_filter_manager_dispose (GObject *object)
{
  LogviewFilterManager* manager;

  manager = LOGVIEW_FILTER_MANAGER (object);

  g_object_unref (manager->priv->builder);

  G_OBJECT_CLASS (logview_filter_manager_parent_class)->dispose (object);
}

static void
logview_filter_manager_response (GtkDialog* dialog, gint response_id)
{
  ctk_widget_destroy (CTK_WIDGET (dialog));
}

static void
logview_filter_manager_class_init (LogviewFilterManagerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkDialogClass *parent_class = CTK_DIALOG_CLASS (klass);

  object_class->dispose = logview_filter_manager_dispose;
  parent_class->response = logview_filter_manager_response;
}

GtkWidget *
logview_filter_manager_new (void)
{
  return g_object_new (LOGVIEW_TYPE_FILTER_MANAGER, NULL);
}
