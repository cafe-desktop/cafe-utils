/* gdict-sidebar.c - sidebar widget
 *
 * Copyright (C) 2006  Emmanuele Bassi <ebassi@gmail.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 *
 * Based on the equivalent widget from Evince
 * 	by Jonathan Blandford,
 * 	Copyright (C) 2004  Red Hat, Inc.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include <ctk/ctk.h>
#include <cdk/cdkkeysyms.h>
#include <glib/gi18n.h>

#include "gdict-sidebar.h"

typedef struct
{
  guint index;

  gchar *id;
  gchar *name;

  CtkWidget *child;
  CtkWidget *menu_item;
} SidebarPage;

struct _GdictSidebarPrivate
{
  GHashTable *pages_by_id;
  GSList *pages;

  CtkWidget *hbox;
  CtkWidget *notebook;
  CtkWidget *menu;
  CtkWidget *close_button;
  CtkWidget *label;
  CtkWidget *select_button;
};

enum
{
  PAGE_CHANGED,
  CLOSED,

  LAST_SIGNAL
};

static guint sidebar_signals[LAST_SIGNAL] = { 0 };
static GQuark sidebar_page_id_quark = 0;

G_DEFINE_TYPE_WITH_PRIVATE (GdictSidebar, gdict_sidebar, CTK_TYPE_BOX);

SidebarPage *
sidebar_page_new (const gchar *id,
		  const gchar *name,
		  CtkWidget   *widget)
{
  SidebarPage *page;

  page = g_slice_new (SidebarPage);

  page->id = g_strdup (id);
  page->name = g_strdup (name);
  page->child = widget;
  page->index = -1;
  page->menu_item = NULL;

  return page;
}

void
sidebar_page_free (SidebarPage *page)
{
  if (G_LIKELY (page))
    {
      g_free (page->name);
      g_free (page->id);

      g_slice_free (SidebarPage, page);
    }
}

static void
gdict_sidebar_finalize (GObject *object)
{
  GdictSidebar *sidebar = GDICT_SIDEBAR (object);
  GdictSidebarPrivate *priv = sidebar->priv;

  if (priv->pages_by_id)
    g_hash_table_destroy (priv->pages_by_id);

  if (priv->pages)
    {
      g_slist_free_full (priv->pages, (GDestroyNotify) sidebar_page_free);
    }

  G_OBJECT_CLASS (gdict_sidebar_parent_class)->finalize (object);
}

static void
gdict_sidebar_dispose (GObject *object)
{
  GdictSidebar *sidebar = GDICT_SIDEBAR (object);

  if (sidebar->priv->menu)
    {
      ctk_menu_detach (CTK_MENU (sidebar->priv->menu));
      sidebar->priv->menu = NULL;
    }

  G_OBJECT_CLASS (gdict_sidebar_parent_class)->dispose (object);
}

static gboolean
gdict_sidebar_select_button_press_cb (CtkWidget      *widget,
				      GdkEventButton *event,
				      gpointer        user_data)
{
  GdictSidebar *sidebar = GDICT_SIDEBAR (user_data);
  CtkAllocation allocation;

  if (event->button == 1)
    {
      CtkRequisition req;
      gint width;

      ctk_widget_get_allocation (widget, &allocation);
      width = allocation.width;
      ctk_widget_set_size_request (sidebar->priv->menu, -1, -1);
      ctk_widget_get_preferred_size (sidebar->priv->menu, &req, NULL);
      ctk_widget_set_size_request (sidebar->priv->menu,
		      		   MAX (width, req.width), -1);
      ctk_widget_grab_focus (widget);

      ctk_toggle_button_set_active (CTK_TOGGLE_BUTTON (widget), TRUE);
      ctk_menu_popup_at_widget (CTK_MENU (sidebar->priv->menu),
                                widget,
                                CDK_GRAVITY_SOUTH_WEST,
                                CDK_GRAVITY_NORTH_WEST,
                                (const GdkEvent*) event);

      return TRUE;
    }

  return FALSE;
}

static gboolean
gdict_sidebar_select_key_press_cb (CtkWidget   *widget,
				   GdkEventKey *event,
				   gpointer     user_data)
{
  GdictSidebar *sidebar = GDICT_SIDEBAR (user_data);

  if (event->keyval == CDK_KEY_space ||
      event->keyval == CDK_KEY_KP_Space ||
      event->keyval == CDK_KEY_Return ||
      event->keyval == CDK_KEY_KP_Enter)
    {
      ctk_toggle_button_set_active (CTK_TOGGLE_BUTTON (widget), TRUE);
      ctk_menu_popup_at_widget (CTK_MENU (sidebar->priv->menu),
                                widget,
                                CDK_GRAVITY_SOUTH_WEST,
                                CDK_GRAVITY_NORTH_WEST,
                                (const GdkEvent*) event);

      return TRUE;
    }

  return FALSE;
}

static void
gdict_sidebar_close_clicked_cb (CtkWidget *widget,
				gpointer   user_data)
{
  GdictSidebar *sidebar = GDICT_SIDEBAR (user_data);

  g_signal_emit (sidebar, sidebar_signals[CLOSED], 0);
}

static void
gdict_sidebar_menu_deactivate_cb (CtkWidget *widget,
				  gpointer   user_data)
{
  GdictSidebar *sidebar = GDICT_SIDEBAR (user_data);
  GdictSidebarPrivate *priv = sidebar->priv;
  CtkToggleButton *select_button = CTK_TOGGLE_BUTTON (priv->select_button);

  ctk_toggle_button_set_active (select_button, FALSE);
}

static void
gdict_sidebar_menu_detach_cb (CtkWidget *widget,
			      CtkMenu   *menu)
{
  GdictSidebar *sidebar = GDICT_SIDEBAR (widget);

  sidebar->priv->menu = NULL;
}

static void
gdict_sidebar_menu_item_activate (CtkWidget *widget,
				  gpointer   user_data)
{
  GdictSidebar *sidebar = GDICT_SIDEBAR (user_data);
  GdictSidebarPrivate *priv = sidebar->priv;
  CtkWidget *menu_item;
  const gchar *id;
  SidebarPage *page;
  gint current_index;

  menu_item = ctk_menu_get_active (CTK_MENU (priv->menu));
  id = g_object_get_qdata (G_OBJECT (menu_item), sidebar_page_id_quark);
  g_assert (id != NULL);

  page = g_hash_table_lookup (priv->pages_by_id, id);
  g_assert (page != NULL);

  current_index = ctk_notebook_get_current_page (CTK_NOTEBOOK (priv->notebook));
  if (current_index == page->index)
    return;

  ctk_notebook_set_current_page (CTK_NOTEBOOK (priv->notebook),
		  		 page->index);
  ctk_label_set_text (CTK_LABEL (priv->label), page->name);

  g_signal_emit (sidebar, sidebar_signals[PAGE_CHANGED], 0);
}

static void
gdict_sidebar_class_init (GdictSidebarClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  sidebar_page_id_quark = g_quark_from_static_string ("gdict-sidebar-page-id");

  gobject_class->finalize = gdict_sidebar_finalize;
  gobject_class->dispose = gdict_sidebar_dispose;

  sidebar_signals[PAGE_CHANGED] =
    g_signal_new ("page-changed",
		  G_TYPE_FROM_CLASS (gobject_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GdictSidebarClass, page_changed),
		  NULL, NULL,
		  g_cclosure_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);
  sidebar_signals[CLOSED] =
    g_signal_new ("closed",
		  G_TYPE_FROM_CLASS (gobject_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GdictSidebarClass, closed),
		  NULL, NULL,
		  g_cclosure_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);
}

static void
gdict_sidebar_init (GdictSidebar *sidebar)
{
  GdictSidebarPrivate *priv;
  CtkWidget *hbox;
  CtkWidget *select_hbox;
  CtkWidget *select_button;
  CtkWidget *close_button;
  CtkWidget *arrow;

  ctk_orientable_set_orientation (CTK_ORIENTABLE (sidebar), CTK_ORIENTATION_VERTICAL);
  sidebar->priv = priv = gdict_sidebar_get_instance_private (sidebar);

  /* we store all the pages inside the list, but we keep
   * a pointer inside the hash table for faster look up
   * times; what's inside the table will be destroyed with
   * the list, so there's no need to supply the destroy
   * functions for keys and values.
   */
  priv->pages = NULL;
  priv->pages_by_id = g_hash_table_new (g_str_hash, g_str_equal);

  /* top option menu */
  hbox = ctk_box_new (CTK_ORIENTATION_HORIZONTAL, 0);
  ctk_box_pack_start (CTK_BOX (sidebar), hbox, FALSE, FALSE, 0);
  ctk_widget_show (hbox);
  priv->hbox = hbox;

  select_button = ctk_toggle_button_new ();
  ctk_button_set_relief (CTK_BUTTON (select_button), CTK_RELIEF_NONE);
  g_signal_connect (select_button, "button-press-event",
		    G_CALLBACK (gdict_sidebar_select_button_press_cb),
		    sidebar);
  g_signal_connect (select_button, "key-press-event",
		    G_CALLBACK (gdict_sidebar_select_key_press_cb),
		    sidebar);
  priv->select_button = select_button;

  select_hbox = ctk_box_new (CTK_ORIENTATION_HORIZONTAL, 0);

  priv->label = ctk_label_new (NULL);
  ctk_label_set_xalign (CTK_LABEL (priv->label), 0.0);
  ctk_label_set_yalign (CTK_LABEL (priv->label), 0.5);
  ctk_box_pack_start (CTK_BOX (select_hbox), priv->label, FALSE, FALSE, 0);
  ctk_widget_show (priv->label);

  arrow = ctk_image_new_from_icon_name ("go-down-symbolic", CTK_ICON_SIZE_BUTTON);
  ctk_box_pack_end (CTK_BOX (select_hbox), arrow, FALSE, FALSE, 0);
  ctk_widget_show (arrow);

  ctk_container_add (CTK_CONTAINER (select_button), select_hbox);
  ctk_widget_show (select_hbox);

  ctk_box_pack_start (CTK_BOX (hbox), select_button, TRUE, TRUE, 0);
  ctk_widget_show (select_button);

  close_button = ctk_button_new ();
  ctk_button_set_relief (CTK_BUTTON (close_button), CTK_RELIEF_NONE);
  ctk_button_set_image (CTK_BUTTON (close_button),
                        ctk_image_new_from_icon_name ("window-close",
                                                      CTK_ICON_SIZE_SMALL_TOOLBAR));
  g_signal_connect (close_button, "clicked",
		    G_CALLBACK (gdict_sidebar_close_clicked_cb),
		    sidebar);
  ctk_box_pack_end (CTK_BOX (hbox), close_button, FALSE, FALSE, 0);
  ctk_widget_show (close_button);
  priv->close_button = close_button;

  sidebar->priv->menu = ctk_menu_new ();
  g_signal_connect (sidebar->priv->menu, "deactivate",
		    G_CALLBACK (gdict_sidebar_menu_deactivate_cb),
		    sidebar);
  ctk_menu_attach_to_widget (CTK_MENU (sidebar->priv->menu),
		  	     CTK_WIDGET (sidebar),
			     gdict_sidebar_menu_detach_cb);
  ctk_widget_show (sidebar->priv->menu);

  sidebar->priv->notebook = ctk_notebook_new ();
  ctk_notebook_set_show_border (CTK_NOTEBOOK (sidebar->priv->notebook), FALSE);
  ctk_notebook_set_show_tabs (CTK_NOTEBOOK (sidebar->priv->notebook), FALSE);
  ctk_box_pack_start (CTK_BOX (sidebar), sidebar->priv->notebook, TRUE, TRUE, 6);
  ctk_widget_show (sidebar->priv->notebook);
}

/*
 * Public API
 */

CtkWidget *
gdict_sidebar_new (void)
{
  return g_object_new (GDICT_TYPE_SIDEBAR, NULL);
}

void
gdict_sidebar_add_page (GdictSidebar *sidebar,
			const gchar  *page_id,
			const gchar  *page_name,
			CtkWidget    *page_widget)
{
  GdictSidebarPrivate *priv;
  SidebarPage *page;
  CtkWidget *menu_item;

  g_return_if_fail (GDICT_IS_SIDEBAR (sidebar));
  g_return_if_fail (page_id != NULL);
  g_return_if_fail (page_name != NULL);
  g_return_if_fail (CTK_IS_WIDGET (page_widget));

  priv = sidebar->priv;

  if (g_hash_table_lookup (priv->pages_by_id, page_id))
    {
      g_warning ("Attempting to add a page to the sidebar with "
		 "id `%s', but there already is a page with the "
		 "same id.  Aborting...",
		 page_id);
      return;
    }

  /* add the page inside the page list */
  page = sidebar_page_new (page_id, page_name, page_widget);

  priv->pages = g_slist_append (priv->pages, page);
  g_hash_table_insert (priv->pages_by_id, page->id, page);

  page->index = ctk_notebook_append_page (CTK_NOTEBOOK (priv->notebook),
		  			  page_widget,
					  NULL);

  /* add the menu item for the page */
  menu_item = ctk_image_menu_item_new_with_label (page_name);
  g_object_set_qdata_full (G_OBJECT (menu_item),
			   sidebar_page_id_quark,
                           g_strdup (page_id),
			   g_free);
  g_signal_connect (menu_item, "activate",
		    G_CALLBACK (gdict_sidebar_menu_item_activate),
		    sidebar);
  ctk_menu_shell_append (CTK_MENU_SHELL (priv->menu), menu_item);
  ctk_widget_show (menu_item);
  page->menu_item = menu_item;

  if (ctk_widget_get_realized (priv->menu))
    ctk_menu_shell_select_item (CTK_MENU_SHELL (priv->menu), menu_item);
  ctk_label_set_text (CTK_LABEL (priv->label), page_name);
  ctk_notebook_set_current_page (CTK_NOTEBOOK (priv->notebook), page->index);
}

void
gdict_sidebar_remove_page (GdictSidebar *sidebar,
			   const gchar  *page_id)
{
  GdictSidebarPrivate *priv;
  SidebarPage *page;
  GList *children, *l;

  g_return_if_fail (GDICT_IS_SIDEBAR (sidebar));
  g_return_if_fail (page_id != NULL);

  priv = sidebar->priv;

  if ((page = g_hash_table_lookup (priv->pages_by_id, page_id)) == NULL)
    {
      g_warning ("Attempting to remove a page from the sidebar with "
		 "id `%s', but there is no page with this id. Aborting...",
		 page_id);
      return;
    }

  children = ctk_container_get_children (CTK_CONTAINER (priv->menu));
  for (l = children; l != NULL; l = l->next)
    {
      CtkWidget *menu_item = l->data;

      if (menu_item == page->menu_item)
        {
          ctk_container_remove (CTK_CONTAINER (priv->menu), menu_item);
	  break;
	}
    }
  g_list_free (children);

  ctk_notebook_remove_page (CTK_NOTEBOOK (priv->notebook), page->index);

  g_hash_table_remove (priv->pages_by_id, page->id);
  priv->pages = g_slist_remove (priv->pages, page);

  sidebar_page_free (page);

  /* select the first page, if present */
  page = priv->pages->data;
  if (page)
    {
      if (ctk_widget_get_realized (priv->menu))
        ctk_menu_shell_select_item (CTK_MENU_SHELL (priv->menu), page->menu_item);
      ctk_label_set_text (CTK_LABEL (priv->label), page->name);
      ctk_notebook_set_current_page (CTK_NOTEBOOK (priv->notebook), page->index);
    }
  else
    ctk_widget_hide (CTK_WIDGET (sidebar));
}

void
gdict_sidebar_view_page (GdictSidebar *sidebar,
			 const gchar  *page_id)
{
  GdictSidebarPrivate *priv;
  SidebarPage *page;

  g_return_if_fail (GDICT_IS_SIDEBAR (sidebar));
  g_return_if_fail (page_id != NULL);

  priv = sidebar->priv;
  page = g_hash_table_lookup (priv->pages_by_id, page_id);
  if (!page)
    return;

  ctk_notebook_set_current_page (CTK_NOTEBOOK (priv->notebook), page->index);
  ctk_label_set_text (CTK_LABEL (priv->label), page->name);
  if (ctk_widget_get_realized (priv->menu))
    ctk_menu_shell_select_item (CTK_MENU_SHELL (priv->menu), page->menu_item);
}

const gchar *
gdict_sidebar_current_page (GdictSidebar *sidebar)
{
  GdictSidebarPrivate *priv;
  gint index;
  SidebarPage *page;

  g_return_val_if_fail (GDICT_IS_SIDEBAR (sidebar), NULL);

  priv = sidebar->priv;

  index = ctk_notebook_get_current_page (CTK_NOTEBOOK (priv->notebook));
  page = g_slist_nth_data (priv->pages, index);
  if (page == NULL)
    return NULL;

  return page->id;
}

gchar **
gdict_sidebar_list_pages (GdictSidebar *sidebar,
                          gsize        *length)
{
  GdictSidebarPrivate *priv;
  gchar **retval;
  gint i;
  GSList *l;

  g_return_val_if_fail (GDICT_IS_SIDEBAR (sidebar), NULL);

  priv = sidebar->priv;

  retval = g_new (gchar*, g_slist_length (priv->pages) + 1);
  for (l = priv->pages, i = 0; l; l = l->next, i++)
    retval[i++] = g_strdup (l->data);

  retval[i] = NULL;

  if (length)
    *length = i;

  return retval;
}
