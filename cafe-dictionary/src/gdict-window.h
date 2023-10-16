/* gdict-window.h - main application window
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

#ifndef __GDICT_WINDOW_H__
#define __GDICT_WINDOW_H__

#include <gio/gio.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <ctk/ctk.h>
#include <libgdict/gdict.h>

G_BEGIN_DECLS

#define GDICT_TYPE_WINDOW	(gdict_window_get_type ())
#define GDICT_WINDOW(obj)	(G_TYPE_CHECK_INSTANCE_CAST ((obj), GDICT_TYPE_WINDOW, GdictWindow))
#define GDICT_IS_WINDOW(obj)	(G_TYPE_CHECK_INSTANCE_TYPE ((obj), GDICT_TYPE_WINDOW))

typedef enum {
  GDICT_WINDOW_ACTION_LOOKUP,
  GDICT_WINDOW_ACTION_MATCH,
  GDICT_WINDOW_ACTION_CLEAR
} GdictWindowAction;

#define GDICT_TYPE_WINDOW_ACTION	(gdict_window_action_get_type ())
GType gdict_window_action_get_type (void) G_GNUC_CONST;

typedef struct _GdictWindow      GdictWindow;
typedef struct _GdictWindowClass GdictWindowClass;

struct _GdictWindow
{
  CtkWindow parent_instance;

  CtkWidget *main_box;
  CtkWidget *menubar;
  CtkWidget *entry;

  /* sidebar widgets */
  CtkWidget *speller;
  CtkWidget *db_chooser;
  CtkWidget *strat_chooser;
  CtkWidget *source_chooser;

  CtkWidget *sidebar;
  CtkWidget *sidebar_frame;

  CtkWidget *defbox;
  CtkWidget *defbox_frame;

  CtkWidget *status;
  CtkWidget *progress;

  CtkUIManager *ui_manager;
  CtkActionGroup *action_group;

  CtkEntryCompletion *completion;
  CtkListStore *completion_model;

  GdictWindowAction action;

  gchar *word;
  gint max_definition;
  gint last_definition;
  gint current_definition;

  gchar *source_name;
  GdictSourceLoader *loader;
  GdictContext *context;
  guint definition_id;
  guint lookup_start_id;
  guint lookup_end_id;
  guint error_id;

  gchar *database;
  gchar *strategy;
  gchar *print_font;
  gchar *defbox_font;

  GSettings *settings;
  GSettings *desktop_settings;

  CdkCursor *busy_cursor;

  gint default_width;
  gint default_height;
  gint current_width;
  gint current_height;
  gint sidebar_width;

  gchar *sidebar_page;

  guint is_maximized      : 1;
  guint sidebar_visible   : 1;
  guint statusbar_visible : 1;
  guint in_construction   : 1;

  gulong window_id;
};

struct _GdictWindowClass
{
  CtkWindowClass parent_class;

  void (*created) (GdictWindow *parent_window,
  		   GdictWindow *new_window);
};

GType      gdict_window_get_type (void) G_GNUC_CONST;
CtkWidget *gdict_window_new      (GdictWindowAction  action,
				  GdictSourceLoader *loader,
				  const gchar       *source_name,
				  const gchar       *word);

#endif /* __GDICT_WINDOW_H__ */
