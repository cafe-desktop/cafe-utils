/* gdict-speller.c - display widget for dictionary matches
 *
 * Copyright (C) 2006  Emmanuele Bassi <ebassi@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 */

/**
 * SECTION:gdict-speller
 * @short_description: Display matching words
 *
 * #GdictSpeller is a widget showing a list of words returned by a
 * #GdictContext query, using a specific database and a matching strategy.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include <cdk/cdkkeysyms.h>
#include <ctk/ctk.h>
#include <glib/gi18n-lib.h>

#include "gdict-speller.h"
#include "gdict-utils.h"
#include "gdict-enum-types.h"
#include "gdict-marshal.h"
#include "gdict-debug.h"
#include "gdict-private.h"

struct _GdictSpellerPrivate
{
  GdictContext *context;
  gchar *database;
  gchar *strategy;

  gchar *word;

  CtkWidget *treeview;
  CtkWidget *clear_button;

  CdkCursor *busy_cursor;

  CtkListStore *store;
  gint results;

  guint start_id;
  guint end_id;
  guint match_id;
  guint error_id;

  guint is_searching : 1;
};

typedef enum
{
  MATCH_DB,
  MATCH_WORD,
  MATCH_ERROR
} MatchType;

enum
{
  MATCH_COLUMN_TYPE,
  MATCH_COLUMN_DB_NAME,
  MATCH_COLUMN_WORD,

  MATCH_N_COLUMNS
};

enum
{
  PROP_0,

  PROP_CONTEXT,
  PROP_WORD,
  PROP_DATABASE,
  PROP_STRATEGY,
  PROP_COUNT
};

enum
{
  WORD_ACTIVATED,

  LAST_SIGNAL
};

static guint speller_signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE_WITH_PRIVATE (GdictSpeller, gdict_speller, CTK_TYPE_BOX)

static void
set_gdict_context (GdictSpeller *speller,
		   GdictContext *context)
{
  GdictSpellerPrivate *priv;

  g_assert (GDICT_IS_SPELLER (speller));

  priv = speller->priv;
  if (priv->context)
    {
      if (priv->start_id)
        {
          GDICT_NOTE (SPELLER, "Removing old context handlers");

          g_signal_handler_disconnect (priv->context, priv->start_id);
          g_signal_handler_disconnect (priv->context, priv->match_id);
          g_signal_handler_disconnect (priv->context, priv->end_id);

          priv->start_id = 0;
          priv->end_id = 0;
          priv->match_id = 0;
        }

      if (priv->error_id)
        {
          g_signal_handler_disconnect (priv->context, priv->error_id);

          priv->error_id = 0;
        }

      GDICT_NOTE (SPELLER, "Removing old context");

      g_object_unref (G_OBJECT (priv->context));
    }

  if (!context)
    return;

  if (!GDICT_IS_CONTEXT (context))
    {
      g_warning ("Object of type `%s' instead of a GdictContext\n",
      		 g_type_name (G_OBJECT_TYPE (context)));
      return;
    }

  GDICT_NOTE (SPELLER, "Setting new context\n");

  priv->context = context;
  g_object_ref (G_OBJECT (priv->context));
}

static void
gdict_speller_finalize (GObject *gobject)
{
  GdictSpeller *speller = GDICT_SPELLER (gobject);
  GdictSpellerPrivate *priv = speller->priv;

  if (priv->context)
    set_gdict_context (speller, NULL);

  if (priv->busy_cursor)
    g_object_unref (priv->busy_cursor);

  g_free (priv->strategy);
  g_free (priv->database);
  g_free (priv->word);

  if (priv->store)
    g_object_unref (priv->store);

  G_OBJECT_CLASS (gdict_speller_parent_class)->finalize (gobject);
}


static void
gdict_speller_set_property (GObject      *gobject,
			    guint         prop_id,
			    const GValue *value,
			    GParamSpec   *pspec)
{
  GdictSpeller *speller = GDICT_SPELLER (gobject);
  GdictSpellerPrivate *priv = speller->priv;

  switch (prop_id)
    {
    case PROP_CONTEXT:
      set_gdict_context (speller, g_value_get_object (value));
      break;
    case PROP_DATABASE:
      g_free (priv->database);
      priv->database = g_strdup (g_value_get_string (value));
      break;
    case PROP_STRATEGY:
      g_free (priv->strategy);
      priv->strategy = g_strdup (g_value_get_string (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
gdict_speller_get_property (GObject    *gobject,
			    guint       prop_id,
			    GValue     *value,
			    GParamSpec *pspec)
{
  GdictSpeller *speller = GDICT_SPELLER (gobject);

  switch (prop_id)
    {
    case PROP_DATABASE:
      g_value_set_string (value, speller->priv->database);
      break;
    case PROP_STRATEGY:
      g_value_set_string (value, speller->priv->strategy);
      break;
    case PROP_CONTEXT:
      g_value_set_object (value, speller->priv->context);
      break;
    case PROP_COUNT:
      g_value_set_int (value, speller->priv->results);
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
row_activated_cb (CtkTreeView       *treeview,
		  CtkTreePath       *path,
		  CtkTreeViewColumn *column,
		  gpointer           user_data)
{
  GdictSpeller *speller = GDICT_SPELLER (user_data);
  GdictSpellerPrivate *priv = speller->priv;
  CtkTreeIter iter;
  gchar *word, *db_name;
  gboolean valid;

  valid = ctk_tree_model_get_iter (CTK_TREE_MODEL (priv->store),
			           &iter,
				   path);
  if (!valid)
    {
      g_warning ("Invalid iterator found");

      return;
    }

  ctk_tree_model_get (CTK_TREE_MODEL (priv->store), &iter,
		      MATCH_COLUMN_WORD, &word,
		      MATCH_COLUMN_DB_NAME, &db_name,
		      -1);
  if (word)
    g_signal_emit (speller, speller_signals[WORD_ACTIVATED], 0,
		   word, db_name);
  else
    {
      gchar *row = ctk_tree_path_to_string (path);

      g_warning ("Row %s activated, but no word attached", row);
      g_free (row);
    }

  g_free (word);
  g_free (db_name);
}

static void
clear_button_clicked_cb (CtkWidget *widget,
			 gpointer   user_data)
{
  GdictSpeller *speller = GDICT_SPELLER (user_data);

  gdict_speller_clear (speller);
}

static GObject *
gdict_speller_constructor (GType                  type,
			   guint                  n_params,
			   GObjectConstructParam *params)
{
  GObject *object;
  GdictSpeller *speller;
  GdictSpellerPrivate *priv;
  CtkWidget *sw;
  CtkCellRenderer *renderer;
  CtkTreeViewColumn *column;
  CtkWidget *hbox;

  object = G_OBJECT_CLASS (gdict_speller_parent_class)->constructor (type,
  						                     n_params,
								     params);
  speller = GDICT_SPELLER (object);
  priv = speller->priv;

  sw = ctk_scrolled_window_new (NULL, NULL);
  ctk_widget_set_vexpand (sw, TRUE);
  ctk_scrolled_window_set_policy (CTK_SCROLLED_WINDOW (sw),
  				  CTK_POLICY_AUTOMATIC,
  				  CTK_POLICY_AUTOMATIC);
  ctk_scrolled_window_set_shadow_type (CTK_SCROLLED_WINDOW (sw),
  				       CTK_SHADOW_IN);
  ctk_box_pack_start (CTK_BOX (speller), sw, TRUE, TRUE, 0);
  ctk_widget_show (sw);

  renderer = ctk_cell_renderer_text_new ();
  column = ctk_tree_view_column_new_with_attributes ("matches",
		  				     renderer,
						     "text", MATCH_COLUMN_WORD,
						     NULL);

  priv->treeview = ctk_tree_view_new ();
  ctk_tree_view_set_model (CTK_TREE_VIEW (priv->treeview),
		           CTK_TREE_MODEL (priv->store));
  ctk_tree_view_set_headers_visible (CTK_TREE_VIEW (priv->treeview), FALSE);
  ctk_tree_view_append_column (CTK_TREE_VIEW (priv->treeview), column);
  g_signal_connect (priv->treeview, "row-activated",
		    G_CALLBACK (row_activated_cb), speller);
  ctk_container_add (CTK_CONTAINER (sw), priv->treeview);
  ctk_widget_show (priv->treeview);

  hbox = ctk_box_new (CTK_ORIENTATION_HORIZONTAL, 0);

  priv->clear_button = ctk_button_new ();
  ctk_button_set_image (CTK_BUTTON (priv->clear_button),
                        ctk_image_new_from_icon_name ("edit-clear",
                                                      CTK_ICON_SIZE_SMALL_TOOLBAR));
  g_signal_connect (priv->clear_button, "clicked",
		    G_CALLBACK (clear_button_clicked_cb),
		    speller);
  ctk_box_pack_start (CTK_BOX (hbox), priv->clear_button, FALSE, FALSE, 0);
  ctk_widget_show (priv->clear_button);
  ctk_widget_set_tooltip_text (priv->clear_button,
                               _("Clear the list of similar words"));

  ctk_box_pack_end (CTK_BOX (speller), hbox, FALSE, FALSE, 0);
  ctk_widget_show (hbox);

  return object;
}

static void
gdict_speller_class_init (GdictSpellerClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->finalize = gdict_speller_finalize;
  gobject_class->set_property = gdict_speller_set_property;
  gobject_class->get_property = gdict_speller_get_property;
  gobject_class->constructor = gdict_speller_constructor;

  g_object_class_install_property (gobject_class,
  				   PROP_CONTEXT,
  				   g_param_spec_object ("context",
  				   			_("Context"),
  				   			_("The GdictContext object used to get the word definition"),
  				   			GDICT_TYPE_CONTEXT,
  				   			(G_PARAM_READABLE | G_PARAM_WRITABLE | G_PARAM_CONSTRUCT)));
  g_object_class_install_property (gobject_class,
		  		   PROP_DATABASE,
				   g_param_spec_string ("database",
					   		_("Database"),
							_("The database used to query the GdictContext"),
							GDICT_DEFAULT_DATABASE,
							(G_PARAM_READABLE | G_PARAM_WRITABLE)));
  g_object_class_install_property (gobject_class,
		  		   PROP_DATABASE,
				   g_param_spec_string ("strategy",
					   		_("Strategy"),
							_("The strategy used to query the GdictContext"),
							GDICT_DEFAULT_STRATEGY,
							(G_PARAM_READABLE | G_PARAM_WRITABLE)));

  speller_signals[WORD_ACTIVATED] =
    g_signal_new ("word-activated",
		  G_OBJECT_CLASS_TYPE (gobject_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GdictSpellerClass, word_activated),
		  NULL, NULL,
		  gdict_marshal_VOID__STRING_STRING,
		  G_TYPE_NONE, 2,
		  G_TYPE_STRING,
		  G_TYPE_STRING);
}

static void
gdict_speller_init (GdictSpeller *speller)
{
  GdictSpellerPrivate *priv;

  speller->priv = priv = gdict_speller_get_instance_private (speller);

  ctk_orientable_set_orientation (CTK_ORIENTABLE (speller), CTK_ORIENTATION_VERTICAL);

  priv->database = NULL;
  priv->strategy = NULL;
  priv->word = NULL;

  priv->results = -1;
  priv->context = NULL;

  priv->store = ctk_list_store_new (MATCH_N_COLUMNS,
		                    G_TYPE_INT,    /* MatchType */
		                    G_TYPE_STRING, /* db_name */
				    G_TYPE_STRING  /* word */);

  priv->start_id = 0;
  priv->end_id = 0;
  priv->match_id = 0;
  priv->error_id = 0;
}

/**
 * gdict_speller_new:
 *
 * FIXME
 *
 * Return value: FIXME
 *
 * Since:
 */
CtkWidget *
gdict_speller_new (void)
{
  return g_object_new (GDICT_TYPE_SPELLER, NULL);
}

/**
 * gdict_speller_new_with_context:
 * @context: a #GdictContext
 *
 * FIXME
 *
 * Return value: FIXME
 *
 * Since:
 */
CtkWidget *
gdict_speller_new_with_context (GdictContext *context)
{
  g_return_val_if_fail (GDICT_IS_CONTEXT (context), NULL);

  return g_object_new (GDICT_TYPE_SPELLER,
                       "context", context,
                       NULL);
}

/**
 * gdict_speller_set_context:
 * @speller: a #GdictSpeller
 * @context: a #GdictContext
 *
 * FIXME
 *
 * Since:
 */
void
gdict_speller_set_context (GdictSpeller *speller,
			   GdictContext *context)
{
  g_return_if_fail (GDICT_IS_SPELLER (speller));
  g_return_if_fail (context == NULL || GDICT_IS_CONTEXT (context));

  set_gdict_context (speller, context);

  g_object_notify (G_OBJECT (speller), "context");
}

/**
 * gdict_speller_get_context:
 * @speller: a #GdictSpeller
 *
 * FIXME
 *
 * Return value: a #GdictContext
 *
 * Since:
 */
GdictContext *
gdict_speller_get_context (GdictSpeller *speller)
{
  g_return_val_if_fail (GDICT_IS_SPELLER (speller), NULL);

  return speller->priv->context;
}

/**
 * gdict_speller_set_database:
 * @speller: a #GdictSpeller
 * @database: FIXME
 *
 * FIXME
 *
 * Since:
 */
void
gdict_speller_set_database (GdictSpeller *speller,
			    const gchar  *database)
{
  GdictSpellerPrivate *priv;

  g_return_if_fail (GDICT_IS_SPELLER (speller));

  priv = speller->priv;

  if (!database || database[0] == '\0')
    database = GDICT_DEFAULT_DATABASE;

  g_free (priv->database);
  priv->database = g_strdup (database);

  g_object_notify (G_OBJECT (speller), "database");
}

/**
 * gdict_speller_get_database:
 * @speller: a #GdictSpeller
 *
 * FIXME
 *
 * Return value: FIXME
 *
 * Since: FIXME
 */
const gchar *
gdict_speller_get_database (GdictSpeller *speller)
{
  g_return_val_if_fail (GDICT_IS_SPELLER (speller), NULL);

  return speller->priv->database;
}

/**
 * gdict_speller_set_strategy:
 * @speller: a #GdictSpeller
 * @strategy: FIXME
 *
 * FIXME
 *
 * Since: FIXME
 */
void
gdict_speller_set_strategy (GdictSpeller *speller,
			    const gchar  *strategy)
{
  GdictSpellerPrivate *priv;

  g_return_if_fail (GDICT_IS_SPELLER (speller));

  priv = speller->priv;

  if (!strategy || strategy[0] == '\0')
    strategy = GDICT_DEFAULT_STRATEGY;

  g_free (priv->strategy);
  priv->strategy = g_strdup (strategy);

  g_object_notify (G_OBJECT (speller), "strategy");
}

/**
 * gdict_speller_get_strategy:
 * @speller: a #GdictSpeller
 *
 * FIXME
 *
 * Return value: FIXME
 *
 * Since: FIXME
 */
const gchar *
gdict_speller_get_strategy (GdictSpeller *speller)
{
  g_return_val_if_fail (GDICT_IS_SPELLER (speller), NULL);

  return speller->priv->strategy;
}

/**
 * gdict_speller_clear:
 * @speller: a #GdictSpeller
 *
 * FIXME
 *
 * Since: FIXME
 */
void
gdict_speller_clear (GdictSpeller *speller)
{
  GdictSpellerPrivate *priv;

  g_return_if_fail (GDICT_IS_SPELLER (speller));
  priv = speller->priv;

  ctk_tree_view_set_model (CTK_TREE_VIEW (priv->treeview), NULL);

  ctk_list_store_clear (priv->store);
  priv->results = -1;

  ctk_tree_view_set_model (CTK_TREE_VIEW (priv->treeview),
		           CTK_TREE_MODEL (priv->store));
}

static void
lookup_start_cb (GdictContext *context,
		 gpointer      user_data)
{
  GdictSpeller *speller = GDICT_SPELLER (user_data);
  GdictSpellerPrivate *priv = speller->priv;

  if (!priv->busy_cursor)
    {
      CdkDisplay *display = ctk_widget_get_display (CTK_WIDGET (speller));
      priv->busy_cursor = cdk_cursor_new_for_display (display, CDK_WATCH);
    }

  if (ctk_widget_get_window (CTK_WIDGET (speller)))
    cdk_window_set_cursor (ctk_widget_get_window (CTK_WIDGET (speller)), priv->busy_cursor);

  priv->is_searching = TRUE;
}

static void
lookup_end_cb (GdictContext *context,
	       gpointer      user_data)
{
  GdictSpeller *speller = GDICT_SPELLER (user_data);
  GdictSpellerPrivate *priv = speller->priv;

  if (ctk_widget_get_window (CTK_WIDGET (speller)))
    cdk_window_set_cursor (ctk_widget_get_window (CTK_WIDGET (speller)), NULL);

  g_free (priv->word);
  priv->word = NULL;

  priv->is_searching = FALSE;
}

static void
match_found_cb (GdictContext *context,
		GdictMatch   *match,
		gpointer      user_data)
{
  GdictSpeller *speller = GDICT_SPELLER (user_data);
  GdictSpellerPrivate *priv = speller->priv;
  CtkTreeIter iter;

  GDICT_NOTE (SPELLER, "MATCH: `%s' (from `%s')",
              gdict_match_get_word (match),
              gdict_match_get_database (match));

  ctk_list_store_append (priv->store, &iter);
  ctk_list_store_set (priv->store, &iter,
		      MATCH_COLUMN_TYPE, MATCH_WORD,
		      MATCH_COLUMN_DB_NAME, gdict_match_get_database (match),
		      MATCH_COLUMN_WORD, gdict_match_get_word (match),
		      -1);

  if (priv->results == -1)
    priv->results = 1;
  else
    priv->results += 1;
}

static void
error_cb (GdictContext *context,
	  const GError *error,
	  gpointer      user_data)
{
  GdictSpeller *speller = GDICT_SPELLER (user_data);
  GdictSpellerPrivate *priv = speller->priv;

  gdict_speller_clear (speller);

  if (ctk_widget_get_window (CTK_WIDGET (speller)))
    cdk_window_set_cursor (ctk_widget_get_window (CTK_WIDGET (speller)), NULL);

  g_free (priv->word);
  priv->word = NULL;

  priv->is_searching = FALSE;
}

/**
 * gdict_speller_match:
 * @speller: a #GdictSpeller
 * @word: FIXME
 *
 * FIXME
 *
 * Since: FIXME
 */
void
gdict_speller_match (GdictSpeller *speller,
		     const gchar  *word)
{
  GdictSpellerPrivate *priv;
  GError *match_error;

  g_return_if_fail (GDICT_IS_SPELLER (speller));
  g_return_if_fail (word != NULL);

  priv = speller->priv;

  if (!priv->context)
    {
      g_warning ("Attempting to match `%s', but no GdictContext "
		 "has been set.  Use gdict_speller_set_context() "
		 "before invoking gdict_speller_match().",
		 word);

      return;
    }

  if (priv->is_searching)
    {
      _gdict_show_error_dialog (NULL,
                                _("Another search is in progress"),
                                _("Please wait until the current search ends."));

      return;
    }

  gdict_speller_clear (speller);

  if (!priv->start_id)
    {
      priv->start_id = g_signal_connect (priv->context, "lookup-start",
		                         G_CALLBACK (lookup_start_cb),
					 speller);
      priv->match_id = g_signal_connect (priv->context, "match-found",
                                         G_CALLBACK (match_found_cb),
					 speller);
      priv->end_id = g_signal_connect (priv->context, "lookup-end",
		      		       G_CALLBACK (lookup_end_cb),
				       speller);
    }

  if (!priv->error_id)
    priv->error_id = g_signal_connect (priv->context, "error",
		    		       G_CALLBACK (error_cb),
				       speller);

  g_free (priv->word);
  priv->word = g_strdup (word);

  match_error = NULL;
  gdict_context_match_word (priv->context,
		  	    priv->database,
			    priv->strategy,
			    priv->word,
			    &match_error);
  if (match_error)
    {
      CtkTreeIter iter;

      ctk_list_store_append (priv->store, &iter);
      ctk_list_store_set (priv->store, &iter,
		          MATCH_COLUMN_TYPE, MATCH_ERROR,
			  MATCH_COLUMN_DB_NAME, _("Error while matching"),
			  MATCH_COLUMN_WORD, NULL,
			  -1);

      g_warning ("Error while matching `%s': %s",
                 priv->word,
                 match_error->message);

      g_error_free (match_error);
    }
}

/**
 * gdict_speller_count_match:
 * @speller: a #GdictSpeller
 *
 * FIXME
 *
 * Return value: FIXME
 *
 * Since: FIXME
 */
gint
gdict_speller_count_matches (GdictSpeller *speller)
{
  g_return_val_if_fail (GDICT_IS_SPELLER (speller), -1);

  return speller->priv->results;
}

/**
 * gdict_speller_get_matches:
 * @speller: a #GdictSpeller
 * @length: FIXME
 *
 * FIXME
 *
 * Return value: FIXME
 *
 * Since: FIXME
 */
gchar **
gdict_speller_get_matches (GdictSpeller *speller,
			   gsize         length)
{
  g_return_val_if_fail (GDICT_IS_SPELLER (speller), NULL);

  return NULL;
}
