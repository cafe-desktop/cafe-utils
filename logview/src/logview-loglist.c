/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/* logview-loglist.c - displays a list of the opened logs
 *
 * Copyright (C) 2005 Vincent Noel <vnoel@cox.net>
 * Copyright (C) 2008 Cosimo Cecchi <cosimoc@gnome.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <ctk/ctk.h>
#include <glib/gi18n.h>

#include "logview-manager.h"
#include "logview-log.h"
#include "logview-utils.h"

#include "logview-loglist.h"

struct _LogviewLoglistPrivate {
  CtkTreeStore *model;
  LogviewManager *manager;
  CtkTreePath *selection;
  gboolean has_day_selection;
};

G_DEFINE_TYPE_WITH_PRIVATE (LogviewLoglist, logview_loglist, CTK_TYPE_TREE_VIEW);

enum {
  LOG_OBJECT = 0,
  LOG_NAME,
  LOG_WEIGHT,
  LOG_WEIGHT_SET,
  LOG_DAY
};

enum {
  DAY_SELECTED,
  DAY_CLEARED,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

static void
save_day_selection (LogviewLoglist *loglist, CtkTreeIter *iter)
{
  if (loglist->priv->selection) {
    ctk_tree_path_free (loglist->priv->selection);
  }

  loglist->priv->selection = ctk_tree_model_get_path
    (CTK_TREE_MODEL (loglist->priv->model), iter);
}

static void
update_days_and_lines_for_log (LogviewLoglist *loglist,
                               CtkTreeIter *log, GSList *days)
{
  gboolean res;
  CtkTreeIter iter, dummy;
  GSList *l;
  int i;
  char date[200];
  Day *day;

  /* if we have some days, we can't remove all the items immediately, otherwise,
   * if the row is expanded, it will be collapsed because there are no items,
   * so we create a dummy entry, remove all the others and then remove the
   * dummy one.
   */
  res = ctk_tree_model_iter_children (CTK_TREE_MODEL (loglist->priv->model),
                                      &iter, log);
  if (res) {
    ctk_tree_store_insert_before (loglist->priv->model, &dummy, log, &iter);
    ctk_tree_store_set (loglist->priv->model, &dummy,
                        LOG_NAME, "", -1);
    do {
      ctk_tree_store_remove (loglist->priv->model, &iter);
    } while (ctk_tree_store_iter_is_valid (loglist->priv->model, &iter));
  }

  for (i = 1, l = days; l; l = l->next) {
    /* now insert all the days */
    day = l->data;

    /* TRANSLATORS: "strftime format options, see the man page for strftime(3) for further information." */
    g_date_strftime (date, 200, _("%A, %e %b"), day->date);

    ctk_tree_store_insert (CTK_TREE_STORE (loglist->priv->model),
                           &iter, log, i);
    ctk_tree_store_set (CTK_TREE_STORE (loglist->priv->model),
                        &iter, LOG_NAME, date, LOG_DAY, day, -1);
    i++;
  }

  if (res) {
    ctk_tree_store_remove (loglist->priv->model, &dummy);
  }
}

static CtkTreeIter *
logview_loglist_find_log (LogviewLoglist *list, LogviewLog *log)
{
  CtkTreeIter iter;
  CtkTreeModel *model;
  CtkTreeIter *retval = NULL;
  LogviewLog *current;

  model = CTK_TREE_MODEL (list->priv->model);

  if (!ctk_tree_model_get_iter_first (model, &iter)) {
    return NULL;
  }

  do {
    ctk_tree_model_get (model, &iter, LOG_OBJECT, &current, -1);
    if (current == log) {
      retval = ctk_tree_iter_copy (&iter);
    }
    if (current)
      g_object_unref (current);
  } while (ctk_tree_model_iter_next (model, &iter) != FALSE && retval == NULL);

  return retval;
}

static void
log_changed_cb (LogviewLog *log,
                gpointer user_data)
{
  LogviewLoglist *list = user_data;
  LogviewLog *active;
  CtkTreeIter *iter;

  active = logview_manager_get_active_log (list->priv->manager);

  if (log == active) {
    g_object_unref (active);
    return;
  }

  iter = logview_loglist_find_log (list, log);

  if (!iter) {
    return;
  }

  /* make the log bold in the list */
  ctk_tree_store_set (list->priv->model, iter,
                      LOG_WEIGHT, PANGO_WEIGHT_BOLD,
                      LOG_WEIGHT_SET, TRUE, -1);

  ctk_tree_iter_free (iter);
}


static void
tree_selection_changed_cb (CtkTreeSelection *selection,
                           gpointer user_data)
{
  LogviewLoglist *list = user_data;
  CtkTreeModel *model;
  CtkTreeIter iter, parent;
  LogviewLog *log;
  gboolean is_bold, is_active;
  Day *day;

  if (!ctk_tree_selection_get_selected (selection, &model, &iter)) {
      return;
  }

  ctk_tree_model_get (model, &iter, LOG_OBJECT, &log,
                      LOG_WEIGHT_SET, &is_bold,
                      LOG_DAY, &day, -1);
  if (log) {
    is_active = logview_manager_log_is_active (list->priv->manager, log);

    if (is_active && list->priv->has_day_selection) {
      list->priv->has_day_selection = FALSE;
      g_signal_emit (list, signals[DAY_CLEARED], 0, NULL);
    } else if (!is_active) {
      logview_manager_set_active_log (list->priv->manager, log);
    }
  } else if (day) {
    list->priv->has_day_selection = TRUE;
    ctk_tree_model_iter_parent (model, &parent, &iter);
    ctk_tree_model_get (model, &parent, LOG_OBJECT, &log, -1);

    if (!logview_manager_log_is_active (list->priv->manager, log)) {
      save_day_selection (list, &iter);
      logview_manager_set_active_log (list->priv->manager, log);
    } else {
      g_signal_emit (list, signals[DAY_SELECTED], 0, day, NULL);
    }
  }

  if (is_bold) {
    ctk_tree_store_set (CTK_TREE_STORE (model), &iter,
                        LOG_WEIGHT_SET, FALSE, -1);
  }

  if (log) {
    g_object_unref (log);
  }
}

static void
manager_active_changed_cb (LogviewManager *manager,
                           LogviewLog *log,
                           LogviewLog *old_log,
                           gpointer user_data)
{
  LogviewLoglist *list = user_data;
  CtkTreeIter * iter, sel_iter;
  CtkTreeSelection * selection;

  if (list->priv->selection &&
      ctk_tree_model_get_iter (CTK_TREE_MODEL (list->priv->model),
                               &sel_iter, list->priv->selection))
  {
    Day *day;

    iter = ctk_tree_iter_copy (&sel_iter);

    ctk_tree_model_get (CTK_TREE_MODEL (list->priv->model), iter,
                        LOG_DAY, &day, -1);

    if (day) {
      g_signal_emit (list, signals[DAY_SELECTED], 0, day, NULL);
    }

    ctk_tree_path_free (list->priv->selection);
    list->priv->selection = NULL;
  } else {
    iter = logview_loglist_find_log (list, log);
  }

  if (!iter) {
    return;
  }

  selection = ctk_tree_view_get_selection (CTK_TREE_VIEW (list));
  g_signal_handlers_block_by_func (selection, tree_selection_changed_cb, list);

  ctk_tree_selection_select_iter (selection, iter);

  g_signal_handlers_unblock_by_func (selection, tree_selection_changed_cb, list);
  ctk_tree_iter_free (iter);
}

static void
manager_log_closed_cb (LogviewManager *manager,
                       LogviewLog *log,
                       gpointer user_data)
{
  LogviewLoglist *list = user_data;
  CtkTreeIter *iter;
  gboolean res;

  iter = logview_loglist_find_log (list, log);

  if (!iter) {
    return;
  }

  g_signal_handlers_disconnect_by_func (log, log_changed_cb, list);

  res = ctk_tree_store_remove (list->priv->model, iter);
  if (res) {
    CtkTreeSelection *selection;

    /* iter now points to the next valid row */
    selection = ctk_tree_view_get_selection (CTK_TREE_VIEW (list));
    ctk_tree_selection_select_iter (selection, iter);
  } else {
    /* FIXME: what shall we do here? */
  }

  ctk_tree_iter_free (iter);
}

static void
manager_log_added_cb (LogviewManager *manager,
                      LogviewLog *log,
                      gpointer user_data)
{
  LogviewLoglist *list = user_data;
  CtkTreeIter iter, child;

  ctk_tree_store_append (list->priv->model, &iter, NULL);
  ctk_tree_store_set (list->priv->model, &iter,
                      LOG_OBJECT, g_object_ref (log),
                      LOG_NAME, logview_log_get_display_name (log), -1);
  if (logview_log_get_has_days (log)) {
    ctk_tree_store_insert (list->priv->model,
                           &child, &iter, 0);
    ctk_tree_store_set (list->priv->model, &child,
                        LOG_NAME, _("Loading..."), -1);
  }

  g_signal_connect (log, "log-changed",
                    G_CALLBACK (log_changed_cb), list);
}

static void
row_expanded_cb (CtkTreeView *view,
                 CtkTreeIter *iter,
                 CtkTreePath *path,
                 gpointer user_data)
{
  LogviewLoglist *list = user_data;
  LogviewLog *log;

  ctk_tree_model_get (CTK_TREE_MODEL (list->priv->model), iter,
                      LOG_OBJECT, &log, -1);
  if (!logview_manager_log_is_active (list->priv->manager, log)) {
    logview_manager_set_active_log (list->priv->manager, log);
  }

  g_object_unref (log);
}

static int
loglist_sort_func (CtkTreeModel *model,
                   CtkTreeIter *a,
                   CtkTreeIter *b,
                   gpointer user_data)
{
  char *name_a, *name_b;
  Day *day_a, *day_b;
  int retval = 0;

  switch (ctk_tree_store_iter_depth (CTK_TREE_STORE (model), a)) {
    case 0:
      ctk_tree_model_get (model, a, LOG_NAME, &name_a, -1);
      ctk_tree_model_get (model, b, LOG_NAME, &name_b, -1);
      retval = g_utf8_collate (name_a, name_b);
      g_free (name_a);
      g_free (name_b);

      break;
    case 1:
      ctk_tree_model_get (model, a, LOG_DAY, &day_a, -1);
      ctk_tree_model_get (model, b, LOG_DAY, &day_b, -1);
      if (day_a && day_b) {
        retval = days_compare (day_a, day_b);
      } else {
        retval = 0;
      }

      break;
    default:
      g_assert_not_reached ();

      break;
  }

  return retval;
}

static void
do_finalize (GObject *obj)
{
  LogviewLoglist *list = LOGVIEW_LOGLIST (obj);

  g_object_unref (list->priv->model);
  list->priv->model = NULL;

  if (list->priv->selection) {
    ctk_tree_path_free (list->priv->selection);
    list->priv->selection = NULL;
  }

  G_OBJECT_CLASS (logview_loglist_parent_class)->finalize (obj);
}

static void
logview_loglist_init (LogviewLoglist *list)
{
  CtkTreeStore *model;
  CtkTreeViewColumn *column;
  CtkTreeSelection *selection;
  CtkCellRenderer *cell;

  list->priv = logview_loglist_get_instance_private (list);
  list->priv->has_day_selection = FALSE;
  list->priv->selection = NULL;

  model = ctk_tree_store_new (5, LOGVIEW_TYPE_LOG, G_TYPE_STRING, G_TYPE_INT,
                              G_TYPE_BOOLEAN, G_TYPE_POINTER);
  ctk_tree_view_set_model (CTK_TREE_VIEW (list), CTK_TREE_MODEL (model));
  list->priv->model = model;
  ctk_tree_view_set_headers_visible (CTK_TREE_VIEW (list), FALSE);

  selection = ctk_tree_view_get_selection (CTK_TREE_VIEW (list));
  ctk_tree_selection_set_mode (selection, CTK_SELECTION_BROWSE);
  g_signal_connect (selection, "changed",
                    G_CALLBACK (tree_selection_changed_cb), list);

  cell = ctk_cell_renderer_text_new ();
  column = ctk_tree_view_column_new ();
  ctk_tree_view_column_pack_start (column, cell, TRUE);
  ctk_tree_view_column_set_attributes (column, cell,
                                       "text", LOG_NAME,
                                       "weight-set", LOG_WEIGHT_SET,
                                       "weight", LOG_WEIGHT,
                                       NULL);

  ctk_tree_sortable_set_sort_column_id (CTK_TREE_SORTABLE (list->priv->model), LOG_NAME, CTK_SORT_ASCENDING);
  ctk_tree_sortable_set_sort_func (CTK_TREE_SORTABLE (list->priv->model),
                                   LOG_NAME,
                                   (CtkTreeIterCompareFunc) loglist_sort_func,
                                   list, NULL);
  ctk_tree_view_append_column (CTK_TREE_VIEW (list), column);
  ctk_tree_view_set_search_column (CTK_TREE_VIEW (list), -1);

  list->priv->manager = logview_manager_get ();

  g_signal_connect (list->priv->manager, "log-added",
                    G_CALLBACK (manager_log_added_cb), list);
  g_signal_connect (list->priv->manager, "log-closed",
                    G_CALLBACK (manager_log_closed_cb), list);
  g_signal_connect_after (list->priv->manager, "active-changed",
                          G_CALLBACK (manager_active_changed_cb), list);
  g_signal_connect (list, "row-expanded",
                    G_CALLBACK (row_expanded_cb), list);
}

static void
logview_loglist_class_init (LogviewLoglistClass *klass)
{
  GObjectClass *oclass = G_OBJECT_CLASS (klass);
  oclass->finalize = do_finalize;

  signals[DAY_SELECTED] = g_signal_new ("day-selected",
                                        G_OBJECT_CLASS_TYPE (oclass),
                                        G_SIGNAL_RUN_LAST,
                                        G_STRUCT_OFFSET (LogviewLoglistClass, day_selected),
                                        NULL, NULL,
                                        g_cclosure_marshal_VOID__POINTER,
                                        G_TYPE_NONE, 1,
                                        G_TYPE_POINTER);

  signals[DAY_CLEARED] = g_signal_new ("day-cleared",
                                       G_OBJECT_CLASS_TYPE (oclass),
                                       G_SIGNAL_RUN_LAST,
                                       G_STRUCT_OFFSET (LogviewLoglistClass, day_cleared),
                                       NULL, NULL,
                                       g_cclosure_marshal_VOID__VOID,
                                       G_TYPE_NONE, 0);
}

/* public methods */

CtkWidget *
logview_loglist_new (void)
{
  CtkWidget *widget;
  widget = g_object_new (LOGVIEW_TYPE_LOGLIST, NULL);
  return widget;
}

void
logview_loglist_update_lines (LogviewLoglist *loglist, LogviewLog *log)
{
  GSList *days;
  CtkTreeIter *parent;

  g_assert (LOGVIEW_IS_LOGLIST (loglist));
  g_assert (LOGVIEW_IS_LOG (log));

  parent = logview_loglist_find_log (loglist, log);

  if (parent) {
    days = logview_log_get_days_for_cached_lines (log);
    update_days_and_lines_for_log (loglist, parent, days);
    ctk_tree_iter_free (parent);
  }
}

