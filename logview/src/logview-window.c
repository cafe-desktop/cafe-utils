/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/* logview-window.c - main window of logview
 *
 * Copyright (C) 1998  Cesar Miquel  <miquel@df.uba.ar>
 * Copyright (C) 2008  Cosimo Cecchi <cosimoc@gnome.org>
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
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include <config.h>
#include <stdlib.h>
#include <string.h>
#include <ctk/ctk.h>
#include <gdk/gdkkeysyms.h>
#include <glib/gi18n.h>

#include "logview-window.h"

#include "logview-loglist.h"
#include "logview-findbar.h"
#include "logview-about.h"
#include "logview-prefs.h"
#include "logview-manager.h"
#include "logview-filter-manager.h"

#define APP_NAME _("System Log Viewer")
#define SEARCH_START_MARK "lw-search-start-mark"
#define SEARCH_END_MARK "lw-search-end-mark"

struct _LogviewWindowPrivate {
  CtkUIManager *ui_manager;
  CtkActionGroup *action_group;
  CtkActionGroup *filter_action_group;

  CtkWidget *find_bar;
  CtkWidget *loglist;
  CtkWidget *sidebar;
  CtkWidget *version_bar;
  CtkWidget *version_selector;
  CtkWidget *hpaned;
  CtkWidget *text_view;
  CtkWidget *statusbar;

  CtkWidget *message_area;
  CtkWidget *message_primary;
  CtkWidget *message_secondary;

  CtkTextTagTable *tag_table;

  int original_fontsize, fontsize;

  LogviewPrefs *prefs;
  LogviewManager *manager;

  gulong monitor_id;
  guint search_timeout_id;

  GCancellable *read_cancellable;

  guint filter_merge_id;
  GList *active_filters;
  gboolean matches_only;
};

G_DEFINE_TYPE_WITH_PRIVATE (LogviewWindow, logview_window, CTK_TYPE_WINDOW);

static void findbar_close_cb  (LogviewFindbar *findbar,
                               gpointer user_data);
static void read_new_lines_cb (LogviewLog *log,
                               const char **lines,
                               GSList *new_days,
                               GError *error,
                               gpointer user_data);

/* private functions */

static void
logview_version_selector_changed (CtkComboBox *version_selector, gpointer user_data)
{

}
#if 0
	LogviewWindow *logview = user_data;
	Log *log = logview->curlog;
	int selected;

    g_assert (LOGVIEW_IS_WINDOW (logview));

	selected = ctk_combo_box_get_active (version_selector);

	if (selected == log->current_version)
		return;

	/* select a new version */
	if (selected == 0) {
		logview_select_log (logview, log->parent_log);
	} else {
		Log *new;
		if (log->parent_log) {
			new = log->parent_log->older_logs[selected];
		} else {
			new = log->older_logs[selected];
		}

		logview_select_log (logview, new);
	}
}

#endif

/* private helpers */

static void
populate_tag_table (CtkTextTagTable *tag_table)
{
  CtkTextTag *tag;

  tag = ctk_text_tag_new ("bold");
  g_object_set (tag, "weight", PANGO_WEIGHT_BOLD,
                "weight-set", TRUE, NULL);

  ctk_text_tag_table_add (tag_table, tag);

  tag = ctk_text_tag_new ("invisible");
  g_object_set (tag, "invisible", TRUE, "invisible-set", TRUE, NULL);
  ctk_text_tag_table_add (tag_table, tag);

  tag = ctk_text_tag_new ("invisible-filter");
  g_object_set (tag, "invisible", TRUE, "invisible-set", TRUE, NULL);
  ctk_text_tag_table_add (tag_table, tag);
}

static void
populate_style_tag_table (LogviewWindow *logview)
{
  CtkTextTagTable *tag_table = logview->priv->tag_table;
  CtkTextTag *tag;
  CtkStyleContext *context;
  GdkRGBA rgba;

  tag = ctk_text_tag_table_lookup (tag_table, "gray");

  if (tag) {
    ctk_text_tag_table_remove (tag_table, tag);
  }

  tag = ctk_text_tag_new ("gray");

  context = ctk_widget_get_style_context (logview->priv->text_view);
  ctk_style_context_save (context);
  ctk_style_context_add_class (context, "dim-label");
  ctk_style_context_get_color (context, CTK_STATE_FLAG_NORMAL, &rgba);
  ctk_style_context_restore (context);

  g_object_set (tag, "foreground-rgba", &rgba, "foreground-set", TRUE, NULL);

  ctk_text_tag_table_add (tag_table, tag);
}

static void
_ctk_text_buffer_apply_tag_to_rectangle (CtkTextBuffer *buffer, int line_start, int line_end,
                                        int offset_start, int offset_end, char *tag_name)
{
  CtkTextIter start, end;
  int line_cur;

  ctk_text_buffer_get_iter_at_line (buffer, &start, line_start);
  ctk_text_buffer_get_iter_at_line (buffer, &end, line_start);

  for (line_cur = line_start; line_cur < line_end + 1; line_cur++) {

    if (offset_start > 0) {
      ctk_text_iter_forward_chars (&start, offset_start);
    }

    ctk_text_iter_forward_chars (&end, offset_end);

    ctk_text_buffer_apply_tag_by_name (buffer, tag_name, &start, &end);

    ctk_text_iter_forward_line (&start);
    ctk_text_iter_forward_line (&end);
  }
}

static void
logview_update_statusbar (LogviewWindow *logview, LogviewLog *active)
{
  GDateTime *date_time;
  char *statusbar_text;
  char *size, *modified, *timestring;
  time_t timestamp;

  if (active == NULL) {
    ctk_statusbar_pop (CTK_STATUSBAR (logview->priv->statusbar), 0);
    return;
  }

  timestamp = logview_log_get_timestamp (active);
  date_time = g_date_time_new_from_unix_local (timestamp);
  timestring = g_date_time_format (date_time, "%c");
  modified = g_strdup_printf (_("last update: %s"), timestring);

  size = g_format_size (logview_log_get_file_size (active));

  statusbar_text = g_strdup_printf (_("%d lines (%s) - %s"),
                                    logview_log_get_cached_lines_number (active),
                                    size, modified);

  ctk_statusbar_pop (CTK_STATUSBAR (logview->priv->statusbar), 0);
  ctk_statusbar_push (CTK_STATUSBAR (logview->priv->statusbar), 0, statusbar_text);

  g_free (size);
  g_free (timestring);
  g_free (modified);
  g_free (statusbar_text);
  g_date_time_unref (date_time);
}

#define DEFAULT_LOGVIEW_FONT "Monospace 10"

static void
logview_set_font (LogviewWindow *logview,
                  const char    *fontname)
{
  PangoFontDescription *font_desc;

  if (fontname == NULL)
    fontname = DEFAULT_LOGVIEW_FONT;

  font_desc = pango_font_description_from_string (fontname);
  if (font_desc) {
    ctk_widget_override_font (logview->priv->text_view, font_desc);
    pango_font_description_free (font_desc);
  }
}

static void
logview_set_fontsize (LogviewWindow *logview, gboolean store)
{
  PangoFontDescription *fontdesc;
  PangoContext *context;
  LogviewWindowPrivate *priv = logview->priv;

  context = ctk_widget_get_pango_context (priv->text_view);
  fontdesc = pango_context_get_font_description (context);
  pango_font_description_set_size (fontdesc, (priv->fontsize) * PANGO_SCALE);
  ctk_widget_override_font (priv->text_view, fontdesc);

  if (store) {
    logview_prefs_store_fontsize (logview->priv->prefs, priv->fontsize);
  }
}

static void
logview_set_window_title (LogviewWindow *logview, const char * log_name)
{
  char *window_title;

  if (log_name) {
    window_title = g_strdup_printf ("%s - %s", log_name, APP_NAME);
  } else {
    window_title = g_strdup_printf (APP_NAME);
  }

  ctk_window_set_title (CTK_WINDOW (logview), window_title);

  g_free (window_title);
}

/* actions callbacks */

static void
open_file_selected_cb (CtkWidget *chooser, gint response, LogviewWindow *logview)
{
  GFile *f;
  char *file_uri;
  LogviewLog *log;

  ctk_widget_hide (CTK_WIDGET (chooser));
  if (response != CTK_RESPONSE_OK) {
	  return;
  }

  f = ctk_file_chooser_get_file (CTK_FILE_CHOOSER (chooser));
  file_uri = g_file_get_uri (f);

  log = logview_manager_get_if_loaded (logview->priv->manager, file_uri);

  g_free (file_uri);

  if (log) {
    logview_manager_set_active_log (logview->priv->manager, log);
    g_object_unref (log);
    goto out;
  }

  logview_manager_add_log_from_gfile (logview->priv->manager, f, TRUE);

out:
  g_object_unref (f);
}

static void
logview_open_log (CtkAction *action, LogviewWindow *logview)
{
  static CtkWidget *chooser = NULL;
  char *active;

  if (chooser == NULL) {
    chooser = ctk_file_chooser_dialog_new (_("Open Log"),
                                           CTK_WINDOW (logview),
                                           CTK_FILE_CHOOSER_ACTION_OPEN,
                                           "ctk-cancel", CTK_RESPONSE_CANCEL,
                                           "ctk-open", CTK_RESPONSE_OK,
                                           NULL);
    ctk_dialog_set_default_response (CTK_DIALOG (chooser), CTK_RESPONSE_OK);
    ctk_window_set_modal (CTK_WINDOW (chooser), TRUE);
    g_signal_connect (chooser, "response",
                      G_CALLBACK (open_file_selected_cb), logview);
    g_signal_connect (chooser, "destroy",
                      G_CALLBACK (ctk_widget_destroyed), &chooser);
    active = logview_prefs_get_active_logfile (logview->priv->prefs);
    if (active != NULL) {
      ctk_file_chooser_set_filename (CTK_FILE_CHOOSER (chooser), active);
      g_free (active);
    }
  }

  ctk_window_present (CTK_WINDOW (chooser));
}

static void
logview_close_log (CtkAction *action, LogviewWindow *logview)
{
  findbar_close_cb (LOGVIEW_FINDBAR (logview->priv->find_bar), logview);
  logview_manager_close_active_log (logview->priv->manager);
}

static void
logview_help (CtkAction *action, CtkWidget *parent_window)
{
  GError *error = NULL;

  ctk_show_uri_on_window (CTK_WINDOW (parent_window),
                "help:cafe-system-log", ctk_get_current_event_time (),
                &error);

  if (error) {
    g_warning (_("There was an error displaying help: %s"), error->message);
    g_error_free (error);
  }
}

static void
logview_bigger_text (CtkAction *action, LogviewWindow *logview)
{
  logview->priv->fontsize = MIN (logview->priv->fontsize + 1, 24);
  logview_set_fontsize (logview, TRUE);
}

static void
logview_smaller_text (CtkAction *action, LogviewWindow *logview)
{
  logview->priv->fontsize = MAX (logview->priv->fontsize-1, 6);
  logview_set_fontsize (logview, TRUE);
}

static void
logview_normal_text (CtkAction *action, LogviewWindow *logview)
{
  logview->priv->fontsize = logview->priv->original_fontsize;
  logview_set_fontsize (logview, TRUE);
}

static void
logview_select_all (CtkAction *action, LogviewWindow *logview)
{
  CtkTextIter start, end;
  CtkTextBuffer *buffer;

  buffer = ctk_text_view_get_buffer (CTK_TEXT_VIEW (logview->priv->text_view));

  ctk_text_buffer_get_bounds (buffer, &start, &end);
  ctk_text_buffer_select_range (buffer, &start, &end);

  ctk_widget_grab_focus (CTK_WIDGET (logview->priv->text_view));
}

static void
logview_copy (CtkAction *action, LogviewWindow *logview)
{
  CtkTextBuffer *buffer;
  CtkClipboard *clipboard;

  buffer = ctk_text_view_get_buffer (CTK_TEXT_VIEW (logview->priv->text_view));
  clipboard = ctk_clipboard_get (GDK_SELECTION_CLIPBOARD);

  ctk_text_buffer_copy_clipboard (buffer, clipboard);

  ctk_widget_grab_focus (CTK_WIDGET (logview->priv->text_view));
}

static void
findbar_close_cb (LogviewFindbar *findbar,
                  gpointer user_data)
{
  ctk_widget_hide (CTK_WIDGET (findbar));
  logview_findbar_set_message (findbar, NULL);
}

static void
logview_search_text (LogviewWindow *logview, gboolean forward)
{
  CtkTextBuffer *buffer;
  CtkTextMark *search_start, *search_end;
  CtkTextIter search, start_m, end_m;
  const char *text;
  gboolean res, wrapped;

  wrapped = FALSE;

  text = logview_findbar_get_text (LOGVIEW_FINDBAR (logview->priv->find_bar));

  if (!text || g_strcmp0 (text, "") == 0) {
    return;
  }

  buffer = ctk_text_view_get_buffer (CTK_TEXT_VIEW (logview->priv->text_view));
  search_start = ctk_text_buffer_get_mark (buffer, SEARCH_START_MARK);
  search_end = ctk_text_buffer_get_mark (buffer, SEARCH_END_MARK);

  if (!search_start) {
    /* this is our first search on the buffer, create a new search mark */
    ctk_text_buffer_get_start_iter (buffer, &search);
    search_start = ctk_text_buffer_create_mark (buffer, SEARCH_START_MARK,
                                                &search, TRUE);
    search_end = ctk_text_buffer_create_mark (buffer, SEARCH_END_MARK,
                                              &search, TRUE);
  } else {
    if (forward) {
      ctk_text_buffer_get_iter_at_mark (buffer, &search, search_end);
    } else {
      ctk_text_buffer_get_iter_at_mark (buffer, &search, search_start);
    }
  }

wrap:

  if (forward) {
    res = ctk_text_iter_forward_search (&search, text, CTK_TEXT_SEARCH_VISIBLE_ONLY, &start_m, &end_m, NULL);
  } else {
    res = ctk_text_iter_backward_search (&search, text, CTK_TEXT_SEARCH_VISIBLE_ONLY, &start_m, &end_m, NULL);
  }

  if (res) {
    ctk_text_buffer_select_range (buffer, &start_m, &end_m);
    ctk_text_buffer_move_mark (buffer, search_start, &start_m);
    ctk_text_buffer_move_mark (buffer, search_end, &end_m);

    ctk_text_view_scroll_mark_onscreen (CTK_TEXT_VIEW (logview->priv->text_view), search_end);

    if (wrapped) {
      logview_findbar_set_message (LOGVIEW_FINDBAR (logview->priv->find_bar), _("Wrapped"));
    }
  } else {
    if (wrapped) {

      CtkTextMark *mark;
      CtkTextIter iter;

      if (ctk_text_buffer_get_has_selection (buffer)) {
        /* unselect */
        mark = ctk_text_buffer_get_mark (buffer, "insert");
        ctk_text_buffer_get_iter_at_mark (buffer, &iter, mark);
        ctk_text_buffer_move_mark_by_name (buffer, "selection_bound", &iter);
      }

      logview_findbar_set_message (LOGVIEW_FINDBAR (logview->priv->find_bar), _("Not found"));
    } else {
      if (forward) {
        ctk_text_buffer_get_start_iter (buffer, &search);
      } else {
        ctk_text_buffer_get_end_iter (buffer, &search);
      }

      wrapped = TRUE;
      goto wrap;
    }
  }
}

static void
findbar_previous_cb (LogviewFindbar *findbar,
                     gpointer user_data)
{
  LogviewWindow *logview = user_data;

  logview_search_text (logview, FALSE);
}

static void
findbar_next_cb (LogviewFindbar *findbar,
                 gpointer user_data)
{
  LogviewWindow *logview = user_data;

  logview_search_text (logview, TRUE);
}

static gboolean
text_changed_timeout_cb (gpointer user_data)
{
  LogviewWindow *logview = user_data;
  CtkTextMark *search_start, *search_end;
  CtkTextIter start;
  CtkTextBuffer *buffer;

  logview->priv->search_timeout_id = 0;

  buffer = ctk_text_view_get_buffer (CTK_TEXT_VIEW (logview->priv->text_view));
  search_start = ctk_text_buffer_get_mark (buffer, SEARCH_START_MARK);
  search_end = ctk_text_buffer_get_mark (buffer, SEARCH_END_MARK);

  if (search_start) {
    /* reset the search mark to the start */
    ctk_text_buffer_get_start_iter (buffer, &start);
    ctk_text_buffer_move_mark (buffer, search_start, &start);
    ctk_text_buffer_move_mark (buffer, search_end, &start);
  }

  logview_findbar_set_message (LOGVIEW_FINDBAR (logview->priv->find_bar), NULL);

  logview_search_text (logview, TRUE);

  return FALSE;
}

static void
findbar_text_changed_cb (LogviewFindbar *findbar,
                         gpointer user_data)
{
  LogviewWindow *logview = user_data;

  if (logview->priv->search_timeout_id != 0) {
    g_source_remove (logview->priv->search_timeout_id);
  }

  logview->priv->search_timeout_id = g_timeout_add (300, text_changed_timeout_cb, logview);
}

static void
logview_search (CtkAction *action, LogviewWindow *logview)
{
  logview_findbar_open (LOGVIEW_FINDBAR (logview->priv->find_bar));
}

static void
filter_buffer (LogviewWindow *logview, gint start_line)
{
  CtkTextBuffer *buffer;
  CtkTextIter start, *end;
  gchar* text;
  GList* cur_filter;
  gboolean matched;
  int lines, i;

  buffer = ctk_text_view_get_buffer (CTK_TEXT_VIEW (logview->priv->text_view));
  lines = ctk_text_buffer_get_line_count (buffer);

  for (i = start_line; i < lines; i++) {
    matched = FALSE;

    ctk_text_buffer_get_iter_at_line (buffer, &start, i);
    end = ctk_text_iter_copy (&start);
    ctk_text_iter_forward_line (end);

    text = ctk_text_buffer_get_text (buffer, &start, end, TRUE);

    for (cur_filter = logview->priv->active_filters; cur_filter != NULL;
         cur_filter = g_list_next (cur_filter))
    {
      if (logview_filter_filter (LOGVIEW_FILTER (cur_filter->data), text)) {
        ctk_text_buffer_apply_tag (buffer,
                                   logview_filter_get_tag (LOGVIEW_FILTER (cur_filter->data)),
                                   &start, end);
        matched = TRUE;
      }
    }

    g_free (text);

    if (!matched && logview->priv->matches_only) {
      ctk_text_buffer_apply_tag_by_name (buffer,
                                         "invisible-filter",
                                         &start, end);
    } else {
      ctk_text_buffer_remove_tag_by_name (buffer,
                                          "invisible-filter",
                                          &start, end);
    }

    ctk_text_iter_free (end);
  }
}

static void
filter_remove (LogviewWindow *logview, LogviewFilter *filter)
{
  CtkTextIter start, end;
  CtkTextBuffer *buffer;

  buffer = ctk_text_view_get_buffer (CTK_TEXT_VIEW (logview->priv->text_view));
  ctk_text_buffer_get_bounds (buffer, &start, &end);

  ctk_text_buffer_remove_tag (buffer, logview_filter_get_tag (filter),
                              &start, &end);
}

static void
on_filter_toggled (CtkToggleAction *action, LogviewWindow *logview)
{
  LogviewWindowPrivate *priv = logview_window_get_instance_private (logview);
  const gchar* name;
  LogviewFilter *filter;

  name = ctk_action_get_name (CTK_ACTION (action));

  if (ctk_toggle_action_get_active (action)) {
    priv->active_filters = g_list_append (priv->active_filters,
                                          logview_prefs_get_filter (priv->prefs,
                                                                    name));
    filter_buffer(logview, 0);
  } else {
    filter = logview_prefs_get_filter (priv->prefs, name);
    priv->active_filters = g_list_remove (priv->active_filters,
                                          filter);

    filter_remove (logview, filter);
  }
}

#define FILTER_PLACEHOLDER "/LogviewMenu/FilterMenu/PlaceholderFilters"
static void
update_filter_menu (LogviewWindow *window)
{
  LogviewWindowPrivate *priv;
  CtkUIManager* ui;
  GList *actions, *l;
  guint id;
  GList *filters;
  CtkTextTagTable *table;
  CtkTextTag *tag;
  CtkToggleAction *action;
  gchar* name;

  priv = logview_window_get_instance_private (window);
  ui = priv->ui_manager;

  g_return_if_fail (priv->filter_action_group != NULL);

  table = priv->tag_table;

  if (priv->filter_merge_id != 0) {
    ctk_ui_manager_remove_ui (ui,
                              priv->filter_merge_id);
  }

  actions = ctk_action_group_list_actions (priv->filter_action_group);

  for (l = actions; l != NULL; l = g_list_next (l)) {
    tag = ctk_text_tag_table_lookup (table, ctk_action_get_name (CTK_ACTION (l->data)));
    ctk_text_tag_table_remove (table, tag);

    g_signal_handlers_disconnect_by_func (CTK_ACTION (l->data),
                                          G_CALLBACK (on_filter_toggled),
                                          window);
    ctk_action_group_remove_action (priv->filter_action_group,
                                    CTK_ACTION (l->data));
  }

  g_list_free (actions);

  filters = logview_prefs_get_filters (logview_prefs_get ());

  id = (g_list_length (filters) > 0) ? ctk_ui_manager_new_merge_id (ui) : 0;

  for (l = filters; l != NULL; l = g_list_next (l)) {
    g_object_get (l->data, "name", &name, NULL);

    action = ctk_toggle_action_new (name, name, NULL, NULL);
    ctk_action_group_add_action (priv->filter_action_group,
                                 CTK_ACTION (action));

    g_signal_connect (action,
                      "toggled",
                      G_CALLBACK (on_filter_toggled),
                      window);

    ctk_ui_manager_add_ui (ui, id, FILTER_PLACEHOLDER,
                           name, name, CTK_UI_MANAGER_MENUITEM, FALSE);
    ctk_text_tag_table_add (table,
                            logview_filter_get_tag (LOGVIEW_FILTER (l->data)));

    g_object_unref (action);
    g_free(name);
  }

  g_list_free (filters);

  priv->filter_merge_id = id;
}

static void
on_logview_filter_manager_response (CtkDialog *dialog,
                                    gint response,
                                    LogviewWindow *logview)
{
  update_filter_menu (logview);

  g_list_free (logview->priv->active_filters);
  logview->priv->active_filters = NULL;
}

static void
logview_manage_filters (CtkAction *action, LogviewWindow *logview)
{
  CtkWidget *manager;

  manager = logview_filter_manager_new ();

  g_signal_connect (manager, "response",
                    G_CALLBACK (on_logview_filter_manager_response),
                    logview);

  ctk_window_set_transient_for (CTK_WINDOW (manager),
                                CTK_WINDOW (logview));
  ctk_widget_show (CTK_WIDGET (manager));
}

static void
logview_about (CtkWidget *widget, CtkWidget *window)
{
  g_return_if_fail (CTK_IS_WINDOW (window));

  char *license_trans = g_strjoin ("\n\n", _(logview_about_license[0]),
                                   _(logview_about_license[1]),
                                   _(logview_about_license[2]), NULL);

#ifdef ENABLE_NLS
  const char **p;
  for (p = logview_about_documenters; *p; ++p)
    *p = _(*p);
#endif

  ctk_show_about_dialog (CTK_WINDOW (window),
                         "program-name", _("System Log Viewer"),
                         "version", VERSION,
                         "title", _("About System Log Viewer"),
                         "copyright", _("Copyright \xc2\xa9 1998-2008 Free Software Foundation, Inc.\n"
                                        "Copyright \xc2\xa9 2011-2020 CAFE developers"),
                         "license", license_trans,
                         "wrap-license", TRUE,
                         "comments", _("A system log viewer for CAFE."),
                         "authors", logview_about_authors,
                         "documenters", logview_about_documenters,
                         "translator_credits", strcmp (logview_about_translator_credits,
                                                       "translator-credits") != 0 ?
                                               logview_about_translator_credits : NULL,
                         "logo_icon_name", "cafe-system-log",
                         NULL);
  g_free (license_trans);

  return;
}

static void
logview_toggle_statusbar (CtkAction *action, LogviewWindow *logview)
{
  if (ctk_widget_get_visible (logview->priv->statusbar))
    ctk_widget_hide (logview->priv->statusbar);
  else
    ctk_widget_show (logview->priv->statusbar);
}

static void
logview_toggle_sidebar (CtkAction *action, LogviewWindow *logview)
{
  if (ctk_widget_get_visible (logview->priv->sidebar))
    ctk_widget_hide (logview->priv->sidebar);
  else
    ctk_widget_show (logview->priv->sidebar);
}

static void
logview_toggle_match_filters (CtkToggleAction *action, LogviewWindow *logview)
{
  logview->priv->matches_only = ctk_toggle_action_get_active (action);
  filter_buffer (logview, 0);
}

/* GObject functions */

/* Menus */

static CtkActionEntry entries[] = {
    { "FileMenu", NULL, N_("_File"), NULL, NULL, NULL },
    { "EditMenu", NULL, N_("_Edit"), NULL, NULL, NULL },
    { "ViewMenu", NULL, N_("_View"), NULL, NULL, NULL },
    { "FilterMenu", NULL, N_("_Filters"), NULL, NULL, NULL },
    { "HelpMenu", NULL, N_("_Help"), NULL, NULL, NULL },

    { "OpenLog", "document-open", N_("_Open..."), "<control>O", N_("Open a log from file"),
      G_CALLBACK (logview_open_log) },
    { "CloseLog", "window-close", N_("_Close"), "<control>W", N_("Close this log"),
      G_CALLBACK (logview_close_log) },
    { "Quit", "application-exit", N_("_Quit"), "<control>Q", N_("Quit the log viewer"),
      G_CALLBACK (ctk_main_quit) },

    { "Copy", "edit-copy", N_("_Copy"), "<control>C", N_("Copy the selection"),
      G_CALLBACK (logview_copy) },
    { "SelectAll", NULL, N_("Select _All"), "<Control>A", N_("Select the entire log"),
      G_CALLBACK (logview_select_all) },
    { "Search", "edit-find", N_("_Find..."), "<control>F", N_("Find a word or phrase in the log"),
      G_CALLBACK (logview_search) },

    { "ViewZoomIn", "zoom-in", N_("Zoom _In"), "<control>plus", N_("Bigger text size"),
      G_CALLBACK (logview_bigger_text)},
    { "ViewZoomOut", "zoom-out", N_("Zoom _Out"), "<control>minus", N_("Smaller text size"),
      G_CALLBACK (logview_smaller_text)},
    { "ViewZoom100", "zoom-original", N_("_Normal Size"), "<control>0", N_("Normal text size"),
      G_CALLBACK (logview_normal_text)},

    { "FilterManage", NULL, N_("Manage Filters"), NULL, N_("Manage filters"),
      G_CALLBACK (logview_manage_filters)},

    { "HelpContents", "help-browser", N_("_Contents"), "F1", N_("Open the help contents for the log viewer"),
      G_CALLBACK (logview_help) },
    { "AboutAction", "help-about", N_("_About"), NULL, N_("Show the about dialog for the log viewer"),
      G_CALLBACK (logview_about) },
};

static CtkToggleActionEntry toggle_entries[] = {
    { "ShowStatusBar", NULL, N_("_Statusbar"), NULL, N_("Show Status Bar"),
      G_CALLBACK (logview_toggle_statusbar), TRUE },
    { "ShowSidebar", NULL, N_("Side _Pane"), "F9", N_("Show Side Pane"),
      G_CALLBACK (logview_toggle_sidebar), TRUE },
    { "FilterMatchOnly", NULL, N_("Show matches only"), NULL, N_("Only show lines that match one of the given filters"),
      G_CALLBACK (logview_toggle_match_filters), FALSE}
};

static gboolean
window_size_changed_cb (CtkWidget *widget, GdkEventConfigure *event,
                        gpointer data)
{
  LogviewWindow *window = data;

  logview_prefs_store_window_size (window->priv->prefs,
                                   event->width, event->height);

  return FALSE;
}

static void
real_select_day (LogviewWindow *logview,
                 GDate *date, int first_line, int last_line)
{
  CtkTextBuffer *buffer;
  CtkTextIter start_iter, end_iter, start_vis, end_vis;
  GdkRectangle visible_rect;

  buffer = ctk_text_view_get_buffer (CTK_TEXT_VIEW (logview->priv->text_view));

  ctk_text_buffer_get_bounds (buffer, &start_iter, &end_iter);
  ctk_text_buffer_get_iter_at_line (buffer, &start_vis, first_line);
  ctk_text_buffer_get_iter_at_line (buffer, &end_vis, last_line + 1);

  /* clear all previous invisible tags */
  ctk_text_buffer_remove_tag_by_name (buffer, "invisible",
                                      &start_iter, &end_iter);

  ctk_text_buffer_apply_tag_by_name (buffer, "invisible",
                                     &start_iter, &start_vis);
  ctk_text_buffer_apply_tag_by_name (buffer, "invisible",
                                     &end_vis, &end_iter);

  /* FIXME: why is this needed to update the view when selecting a day back? */
  ctk_text_view_get_visible_rect (CTK_TEXT_VIEW (logview->priv->text_view),
                                  &visible_rect);
  gdk_window_invalidate_rect (ctk_widget_get_window (logview->priv->text_view),
                              &visible_rect, TRUE);
}

static void
loglist_day_selected_cb (LogviewLoglist *loglist,
                         Day *day,
                         gpointer user_data)
{
  LogviewWindow *logview = user_data;

  real_select_day (logview, day->date, day->first_line, day->last_line);
}

static void
loglist_day_cleared_cb (LogviewLoglist *loglist,
                        gpointer user_data)
{
  LogviewWindow *logview = user_data;
  CtkTextBuffer *buffer;
  CtkTextIter start, end;

  buffer = ctk_text_view_get_buffer (CTK_TEXT_VIEW (logview->priv->text_view));
  ctk_text_buffer_get_bounds (buffer, &start, &end);

  /* clear all previous invisible tags */
  ctk_text_buffer_remove_tag_by_name (buffer, "invisible",
                                      &start, &end);
}

static void
logview_window_schedule_log_read (LogviewWindow *window,
                                  LogviewLog *log)
{
  if (window->priv->read_cancellable != NULL) {
    g_cancellable_cancel (window->priv->read_cancellable);
    g_clear_object (&window->priv->read_cancellable);
  }

  window->priv->read_cancellable = g_cancellable_new ();
  logview_log_read_new_lines (log,
                              window->priv->read_cancellable,
                              (LogviewNewLinesCallback) read_new_lines_cb,
                              window);
}

static void
log_monitor_changed_cb (LogviewLog *log,
                        gpointer user_data)
{
  LogviewWindow *window = user_data;

  /* reschedule a read */
  logview_window_schedule_log_read (window, log);
}

static void
paint_timestamps (CtkTextBuffer *buffer, int old_line_count,
                  GSList *days)
{
  GSList *l;

  for (l = days; l; l = l->next) {
    Day *day = l->data;

    _ctk_text_buffer_apply_tag_to_rectangle (buffer,
                                             old_line_count + day->first_line - 1,
                                             old_line_count + day->last_line,
                                             0, day->timestamp_len, "gray");
  }
}

static void
read_new_lines_cb (LogviewLog *log,
                   const char **lines,
                   GSList *new_days,
                   GError *error,
                   gpointer user_data)
{
  LogviewWindow *window = user_data;
  CtkTextBuffer *buffer;
  gboolean boldify = FALSE;
  int i, old_line_count, filter_start_line;
  CtkTextIter iter, start;
  CtkTextMark *mark;
  char *converted, *primary;
  gsize len;

  if (error != NULL) {
    if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
      primary = g_strdup_printf (_("Can't read from \"%s\""),
                                 logview_log_get_display_name (log));
      logview_window_add_error (window, primary, error->message);
      g_free (primary);
    }

    return;
  }

  if (lines == NULL) {
    /* there's no error, but no lines have been read */
    return;
  }

  buffer = ctk_text_view_get_buffer (CTK_TEXT_VIEW (window->priv->text_view));
  old_line_count = ctk_text_buffer_get_line_count (buffer);
  filter_start_line = old_line_count > 0 ? (old_line_count - 1) : 0;

  if (ctk_text_buffer_get_char_count (buffer) != 0) {
    boldify = TRUE;
  }

  ctk_text_buffer_get_end_iter (buffer, &iter);

  if (boldify) {
    mark = ctk_text_buffer_create_mark (buffer, NULL, &iter, TRUE);
  }

  for (i = 0; lines[i]; i++) {
    len = strlen (lines[i]);

    if (!g_utf8_validate (lines[i], len, NULL)) {
      converted = g_locale_to_utf8 (lines[i], (gssize) len, NULL, &len, NULL);
      ctk_text_buffer_insert (buffer, &iter, converted, len);
      g_free (converted);
    } else {
      ctk_text_buffer_insert (buffer, &iter, lines[i], strlen (lines[i]));
    }

    ctk_text_iter_forward_to_end (&iter);
    ctk_text_buffer_insert (buffer, &iter, "\n", 1);
    ctk_text_iter_forward_char (&iter);
  }

  if (boldify) {
    ctk_text_buffer_get_iter_at_mark (buffer, &start, mark);
    ctk_text_buffer_apply_tag_by_name (buffer, "bold", &start, &iter);
    ctk_text_buffer_delete_mark (buffer, mark);
  }
  filter_buffer (window, filter_start_line);

  ctk_text_view_scroll_to_iter (CTK_TEXT_VIEW (window->priv->text_view),
                                &iter, 0.0, FALSE, 0.0, 0.0);

  paint_timestamps (buffer, old_line_count, new_days);

  if (window->priv->monitor_id == 0) {
    window->priv->monitor_id = g_signal_connect (log, "log-changed",
                                                 G_CALLBACK (log_monitor_changed_cb), window);
  }

  logview_update_statusbar (window, log);
  logview_loglist_update_lines (LOGVIEW_LOGLIST (window->priv->loglist), log);
}

static void
active_log_changed_cb (LogviewManager *manager,
                       LogviewLog *log,
                       LogviewLog *old_log,
                       gpointer data)
{
  LogviewWindow *window = data;
  const char **lines;
  CtkTextBuffer *buffer;

  findbar_close_cb (LOGVIEW_FINDBAR (window->priv->find_bar),
                    window);

  logview_set_window_title (window, logview_log_get_display_name (log));

  if (window->priv->monitor_id) {
    g_signal_handler_disconnect (old_log, window->priv->monitor_id);
    window->priv->monitor_id = 0;
  }

  lines = logview_log_get_cached_lines (log);
  buffer = ctk_text_buffer_new (window->priv->tag_table);

  if (lines != NULL) {
    int i;
    CtkTextIter iter;

    /* update the text view to show the current lines */
    ctk_text_buffer_get_end_iter (buffer, &iter);

    for (i = 0; lines[i]; i++) {
      ctk_text_buffer_insert (buffer, &iter, lines[i], strlen (lines[i]));
      ctk_text_iter_forward_to_end (&iter);
      ctk_text_buffer_insert (buffer, &iter, "\n", 1);
      ctk_text_iter_forward_char (&iter);
    }

    paint_timestamps (buffer, 1, logview_log_get_days_for_cached_lines (log));
  }

  if (lines == NULL || logview_log_has_new_lines (log)) {
    /* read the new lines */
    logview_window_schedule_log_read (window, log);
  } else {
    /* start now monitoring the log for changes */
    window->priv->monitor_id = g_signal_connect (log, "log-changed",
                                                 G_CALLBACK (log_monitor_changed_cb), window);
  }

  /* we set the buffer to the view anyway;
   * if there are no lines it will be empty for the duration of the thread
   * and will help us to distinguish the two cases of the following if
   * cause in the callback.
   */
  ctk_text_view_set_buffer (CTK_TEXT_VIEW (window->priv->text_view), buffer);
  g_object_unref (buffer);
}

static void
font_changed_cb (LogviewPrefs *prefs,
                 const char *font_name,
                 gpointer user_data)
{
  LogviewWindow *window = user_data;

  logview_set_font (window, font_name);
}

static void
tearoff_changed_cb (LogviewPrefs *prefs,
                    gboolean have_tearoffs,
                    gpointer user_data)
{
  LogviewWindow *window = user_data;

  ctk_ui_manager_set_add_tearoffs (window->priv->ui_manager, have_tearoffs);
}

static const struct {
  guint keyval;
  GdkModifierType modifier;
  const gchar *action;
} extra_keybindings [] = {
  { GDK_KEY_KP_Add,      GDK_CONTROL_MASK, "ViewZoomIn" },
  { GDK_KEY_KP_Subtract, GDK_CONTROL_MASK, "ViewZoomOut" },
  { GDK_KEY_KP_0,        GDK_CONTROL_MASK, "ViewZoom100" }
};

static gboolean
key_press_event_cb (CtkWidget *widget,
                    GdkEventKey *event,
                    gpointer user_data)
{
  LogviewWindow *window = user_data;
  guint modifier = event->state & ctk_accelerator_get_default_mod_mask ();
  CtkAction *action;
  int i;

  /* handle accelerators that we want bound, but aren't associated with
   * an action */
  for (i = 0; i < G_N_ELEMENTS (extra_keybindings); i++) {
    if (event->keyval == extra_keybindings[i].keyval &&
        modifier == extra_keybindings[i].modifier) {

      action = ctk_action_group_get_action (window->priv->action_group,
                                            extra_keybindings[i].action);
      ctk_action_activate (action);
      return TRUE;
    }
  }

  return FALSE;
}

/* adapted from GEdit */

static void
message_area_create_error_box (LogviewWindow *window,
                               CtkWidget *message_area)
{
  CtkWidget *hbox_content;
  CtkWidget *image;
  CtkWidget *vbox;
  CtkWidget *primary_label;
  CtkWidget *secondary_label;

  hbox_content = ctk_box_new (CTK_ORIENTATION_HORIZONTAL, 8);
  ctk_widget_show (hbox_content);

  image = ctk_image_new_from_icon_name ("dialog-error",
                                        CTK_ICON_SIZE_DIALOG);
  ctk_widget_show (image);
  ctk_box_pack_start (CTK_BOX (hbox_content), image, FALSE, FALSE, 0);
  ctk_widget_set_halign (image, CTK_ALIGN_CENTER);
  ctk_widget_set_valign (image, CTK_ALIGN_START);

  vbox = ctk_box_new (CTK_ORIENTATION_VERTICAL, 6);
  ctk_widget_show (vbox);
  ctk_box_pack_start (CTK_BOX (hbox_content), vbox, TRUE, TRUE, 0);

  primary_label = ctk_label_new (NULL);
  ctk_widget_show (primary_label);
  ctk_box_pack_start (CTK_BOX (vbox), primary_label, TRUE, TRUE, 0);
  ctk_label_set_use_markup (CTK_LABEL (primary_label), TRUE);
  ctk_label_set_line_wrap (CTK_LABEL (primary_label), TRUE);
  ctk_label_set_xalign (CTK_LABEL (primary_label), 0.0);
  ctk_label_set_yalign (CTK_LABEL (primary_label), 0.5);
  ctk_widget_set_can_focus (primary_label, TRUE);
  ctk_label_set_selectable (CTK_LABEL (primary_label), TRUE);

  window->priv->message_primary = primary_label;

  secondary_label = ctk_label_new (NULL);
  ctk_widget_show (secondary_label);
  ctk_box_pack_start (CTK_BOX (vbox), secondary_label, TRUE, TRUE, 0);
  ctk_widget_set_can_focus (secondary_label, TRUE);
  ctk_label_set_use_markup (CTK_LABEL (secondary_label), TRUE);
  ctk_label_set_line_wrap (CTK_LABEL (secondary_label), TRUE);
  ctk_label_set_selectable (CTK_LABEL (secondary_label), TRUE);
  ctk_label_set_xalign (CTK_LABEL (secondary_label), 0.0);
  ctk_label_set_yalign (CTK_LABEL (secondary_label), 0.5);

  window->priv->message_secondary = secondary_label;

  ctk_container_add
      (CTK_CONTAINER (ctk_info_bar_get_content_area
                      (CTK_INFO_BAR (message_area))),
       hbox_content);
}

static void
message_area_set_labels (LogviewWindow *window,
                         const char *primary,
                         const char *secondary)
{
  char *primary_markup, *secondary_markup;

  primary_markup = g_markup_printf_escaped ("<b>%s</b>", primary);
  secondary_markup = g_markup_printf_escaped ("<small>%s</small>",
                                              secondary);

  ctk_label_set_markup (CTK_LABEL (window->priv->message_primary),
                        primary_markup);
  ctk_label_set_markup (CTK_LABEL (window->priv->message_secondary),
                        secondary_markup);

  g_free (primary_markup);
  g_free (secondary_markup);
}

static void
message_area_response_cb (CtkInfoBar *message_area,
                          int response_id, gpointer user_data)
{
  ctk_widget_hide (CTK_WIDGET (message_area));

  g_signal_handlers_disconnect_by_func (message_area,
                                        message_area_response_cb,
                                        user_data);
}

static void
logview_window_finalize (GObject *object)
{
  LogviewWindow *logview = LOGVIEW_WINDOW (object);

  if (logview->priv->read_cancellable != NULL) {
    g_cancellable_cancel (logview->priv->read_cancellable);
    g_clear_object (&logview->priv->read_cancellable);
  }

  g_object_unref (logview->priv->ui_manager);
  G_OBJECT_CLASS (logview_window_parent_class)->finalize (object);
}

static void
logview_window_init (LogviewWindow *logview)
{
  CtkActionGroup *action_group;
  CtkAccelGroup *accel_group;
  GError *error = NULL;
  CtkWidget *hpaned, *main_view, *vbox, *w;
  PangoContext *context;
  PangoFontDescription *fontdesc;
  gchar *monospace_font_name;
  LogviewWindowPrivate *priv;
  int width, height;
  gboolean res;

  CtkStyleContext *s_context;

  s_context = ctk_widget_get_style_context (CTK_WIDGET (logview));
  ctk_style_context_add_class (s_context, "logview-window");

  priv = logview->priv = logview_window_get_instance_private (logview);
  priv->prefs = logview_prefs_get ();
  priv->manager = logview_manager_get ();
  priv->monitor_id = 0;

  logview_prefs_get_stored_window_size (priv->prefs, &width, &height);
  ctk_window_set_default_size (CTK_WINDOW (logview), width, height);

  vbox = ctk_box_new (CTK_ORIENTATION_VERTICAL, 0);
  ctk_container_add (CTK_CONTAINER (logview), vbox);

  /* create menus */
  action_group = ctk_action_group_new ("LogviewMenuActions");
  ctk_action_group_set_translation_domain (action_group, NULL);
  ctk_action_group_add_actions (action_group, entries, G_N_ELEMENTS (entries), logview);
  ctk_action_group_add_toggle_actions (action_group, toggle_entries, G_N_ELEMENTS (toggle_entries), logview);
  priv->action_group = action_group;

  priv->ui_manager = ctk_ui_manager_new ();

  ctk_ui_manager_insert_action_group (priv->ui_manager, action_group, 0);
  accel_group = ctk_ui_manager_get_accel_group (priv->ui_manager);
  ctk_window_add_accel_group (CTK_WINDOW (logview), accel_group);

  res = ctk_ui_manager_add_ui_from_resource (priv->ui_manager,
                                             "/org/cafe/system-log/logview-toolbar.xml",
                                             &error);

  if (res == FALSE) {
    priv->ui_manager = NULL;
    g_critical ("Can't load the UI description: %s", error->message);
    g_error_free (error);
    return;
  }

  ctk_ui_manager_set_add_tearoffs (priv->ui_manager,
                                   logview_prefs_get_have_tearoff (priv->prefs));

  w = ctk_ui_manager_get_widget (priv->ui_manager, "/LogviewMenu");
  ctk_box_pack_start (CTK_BOX (vbox), w, FALSE, FALSE, 0);
  ctk_widget_show (w);

  /* panes */
  hpaned = ctk_paned_new (CTK_ORIENTATION_HORIZONTAL);
  ctk_box_pack_start (CTK_BOX (vbox), hpaned, TRUE, TRUE, 0);
  priv->hpaned = hpaned;
  ctk_widget_show (hpaned);

  /* first pane : sidebar (list of logs) */
  priv->sidebar = ctk_box_new (CTK_ORIENTATION_VERTICAL, 0);
  ctk_widget_show (priv->sidebar);

  /* first pane: log list */
  w = ctk_scrolled_window_new (NULL, NULL);
  ctk_scrolled_window_set_policy (CTK_SCROLLED_WINDOW (w),
                                  CTK_POLICY_NEVER, CTK_POLICY_AUTOMATIC);
  ctk_scrolled_window_set_shadow_type (CTK_SCROLLED_WINDOW (w),
                                       CTK_SHADOW_ETCHED_IN);

  priv->loglist = logview_loglist_new ();
  ctk_container_add (CTK_CONTAINER (w), priv->loglist);
  ctk_box_pack_start (CTK_BOX (priv->sidebar), w, TRUE, TRUE, 0);
  ctk_paned_pack1 (CTK_PANED (hpaned), priv->sidebar, FALSE, FALSE);
  ctk_widget_show (w);
  ctk_widget_show (priv->loglist);

  g_signal_connect (priv->loglist, "day_selected",
                    G_CALLBACK (loglist_day_selected_cb), logview);
  g_signal_connect (priv->loglist, "day_cleared",
                    G_CALLBACK (loglist_day_cleared_cb), logview);

  /* second pane: log */
  main_view = ctk_box_new (CTK_ORIENTATION_VERTICAL, 0);
  ctk_paned_pack2 (CTK_PANED (hpaned), main_view, TRUE, TRUE);

  /* second pane: error message area */
  priv->message_area = ctk_info_bar_new ();
  message_area_create_error_box (logview, priv->message_area);
  ctk_info_bar_add_button (CTK_INFO_BAR (priv->message_area),
                           "ctk-close", CTK_RESPONSE_CLOSE);
  ctk_box_pack_start (CTK_BOX (main_view), priv->message_area, FALSE, FALSE, 0);

  /* second pane: text view */
  w = ctk_scrolled_window_new (NULL, NULL);
  ctk_scrolled_window_set_policy (CTK_SCROLLED_WINDOW (w),
                                  CTK_POLICY_AUTOMATIC, CTK_POLICY_AUTOMATIC);
  ctk_scrolled_window_set_shadow_type (CTK_SCROLLED_WINDOW (w), CTK_SHADOW_IN);
  ctk_box_pack_start (CTK_BOX (main_view), w, TRUE, TRUE, 0);
  ctk_widget_show (w);

  priv->tag_table = ctk_text_tag_table_new ();
  populate_tag_table (priv->tag_table);

  priv->text_view = ctk_text_view_new ();
  g_object_set (priv->text_view, "editable", FALSE, NULL);
  populate_style_tag_table (logview);

  ctk_container_add (CTK_CONTAINER (w), priv->text_view);
  ctk_widget_show (priv->text_view);

  /* use the desktop monospace font */
  monospace_font_name = logview_prefs_get_monospace_font_name (priv->prefs);
  logview_set_font (logview, monospace_font_name);
  g_free (monospace_font_name);

  /* remember the original font size */
  context = ctk_widget_get_pango_context (priv->text_view);
  fontdesc = pango_context_get_font_description (context);
  priv->original_fontsize = pango_font_description_get_size (fontdesc) / PANGO_SCALE;

  /* restore saved zoom */
  priv->fontsize = logview_prefs_get_stored_fontsize (priv->prefs);

  if (priv->fontsize <= 0) {
    /* restore the default */
    logview_normal_text (NULL, logview);
  } else {
    logview_set_fontsize (logview, FALSE);
  }

  /* version selector */
  priv->version_bar = ctk_box_new (CTK_ORIENTATION_HORIZONTAL, 0);
  ctk_container_set_border_width (CTK_CONTAINER (priv->version_bar), 3);
  priv->version_selector = ctk_combo_box_text_new ();
  g_signal_connect (priv->version_selector, "changed",
                    G_CALLBACK (logview_version_selector_changed), logview);
  w = ctk_label_new (_("Version: "));

  ctk_box_pack_end (CTK_BOX (priv->version_bar), priv->version_selector, FALSE, FALSE, 0);
  ctk_box_pack_end (CTK_BOX (priv->version_bar), w, FALSE, FALSE, 0);
  ctk_box_pack_end (CTK_BOX (main_view), priv->version_bar, FALSE, FALSE, 0);

  priv->find_bar = logview_findbar_new ();
  ctk_box_pack_end (CTK_BOX (main_view), priv->find_bar, FALSE, FALSE, 0);

  g_signal_connect (priv->find_bar, "previous",
                    G_CALLBACK (findbar_previous_cb), logview);
  g_signal_connect (priv->find_bar, "next",
                    G_CALLBACK (findbar_next_cb), logview);
  g_signal_connect (priv->find_bar, "text_changed",
                    G_CALLBACK (findbar_text_changed_cb), logview);
  g_signal_connect (priv->find_bar, "close",
                    G_CALLBACK (findbar_close_cb), logview);

  /* signal handlers
   * - first is used to remember/restore the window size on quit.
   */
  g_signal_connect (logview, "configure_event",
                    G_CALLBACK (window_size_changed_cb), logview);
  g_signal_connect (priv->prefs, "system-font-changed",
                    G_CALLBACK (font_changed_cb), logview);
  g_signal_connect (priv->prefs, "have-tearoff-changed",
                    G_CALLBACK (tearoff_changed_cb), logview);
  g_signal_connect (priv->manager, "active-changed",
                    G_CALLBACK (active_log_changed_cb), logview);
  g_signal_connect (logview, "key-press-event",
                    G_CALLBACK (key_press_event_cb), logview);

  /* status area at bottom */
  priv->statusbar = ctk_statusbar_new ();
	ctk_widget_set_margin_top (CTK_WIDGET (logview->priv->statusbar), 0);
	ctk_widget_set_margin_bottom (CTK_WIDGET (logview->priv->statusbar), 0);

  ctk_box_pack_start (CTK_BOX (vbox), priv->statusbar, FALSE, FALSE, 0);
  ctk_widget_show (priv->statusbar);

  /* Filter menu */
  priv->filter_action_group = ctk_action_group_new ("ActionGroupFilter");
  ctk_ui_manager_insert_action_group (priv->ui_manager, priv->filter_action_group,
                                      1);
  priv->active_filters = NULL;
  update_filter_menu (logview);

  ctk_widget_show (vbox);
  ctk_widget_show (main_view);
}

static void
logview_window_class_init (LogviewWindowClass *klass)
{
  GObjectClass *object_class = (GObjectClass *) klass;

  object_class->finalize = logview_window_finalize;
}

/* public methods */

CtkWidget *
logview_window_new ()
{
  LogviewWindow *logview;

  logview = g_object_new (LOGVIEW_TYPE_WINDOW, NULL);

  if (logview->priv->ui_manager == NULL) {
    return NULL;
  }

  return CTK_WIDGET (logview);
}

void
logview_window_add_error (LogviewWindow *window,
                          const char *primary,
                          const char *secondary)
{
  LogviewWindowPrivate *priv;

  g_assert (LOGVIEW_IS_WINDOW (window));
  priv = window->priv;

  message_area_set_labels (window,
                           primary, secondary);

  ctk_widget_show (priv->message_area);

  g_signal_connect (priv->message_area, "response",
                    G_CALLBACK (message_area_response_cb), window);
}

void
logview_window_add_errors (LogviewWindow *window,
                           GPtrArray *errors)
{
  char *primary, *secondary;
  GString *str;
  char **err;
  int i;

  g_assert (LOGVIEW_IS_WINDOW (window));
  g_assert (errors->len > 1);

  primary = g_strdup (_("Could not open the following files:"));
  str = g_string_new (NULL);

  for (i = 0; i < errors->len; i++) {
    err = (char **) g_ptr_array_index (errors, i);
    g_string_append (str, err[0]);
    g_string_append (str, ": ");
    g_string_append (str, err[1]);
    g_string_append (str, "\n");
  }

  secondary = g_string_free (str, FALSE);

  message_area_set_labels (window, primary, secondary);

  ctk_widget_show (window->priv->message_area);

  g_signal_connect (window->priv->message_area, "response",
                    G_CALLBACK (message_area_response_cb), window);

  g_free (primary);
  g_free (secondary);
}


