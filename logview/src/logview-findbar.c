/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/* logview-findbar.c - find toolbar for logview
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
#include <cdk/cdkkeysyms.h>

#include "logview-findbar.h"

struct _LogviewFindbarPrivate {
  CtkWidget *entry;
  CtkWidget *message;

  CtkToolItem *clear_button;
  CtkToolItem *back_button;
  CtkToolItem *forward_button;
  CtkToolItem *status_item;
  CtkToolItem *separator;

  char *string;

  guint status_bold_id;
};

enum {
  PREVIOUS,
  NEXT,
  CLOSE,
  TEXT_CHANGED,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE_WITH_PRIVATE (LogviewFindbar, logview_findbar, CTK_TYPE_TOOLBAR);

static void
back_button_clicked_cb (CtkToolButton *button,
                        gpointer user_data)
{
  LogviewFindbar *findbar = user_data;

  g_signal_emit (findbar, signals[PREVIOUS], 0);
}

static void
forward_button_clicked_cb (CtkToolButton *button,
                           gpointer user_data)
{
  LogviewFindbar *findbar = user_data;

  g_signal_emit (findbar, signals[NEXT], 0);
}

static void
clear_button_clicked_cb (CtkToolButton *button,
                         gpointer user_data)
{
  LogviewFindbar *findbar = user_data;

  logview_findbar_set_message (findbar, NULL);
  ctk_entry_set_text (CTK_ENTRY (findbar->priv->entry), "");
}

static void
entry_activate_cb (CtkWidget *entry,
                   gpointer user_data)
{
  LogviewFindbar *findbar = user_data;

  g_signal_emit (findbar, signals[NEXT], 0);
}

static void
entry_changed_cb (CtkEditable *editable,
                  gpointer user_data)
{
  LogviewFindbar *findbar = user_data;
  const char *text;

  text = ctk_entry_get_text (CTK_ENTRY (editable));

  if (g_strcmp0 (text, "") == 0) {
    return;
  }

  if (g_strcmp0 (findbar->priv->string, text) != 0) {
    g_free (findbar->priv->string);
    findbar->priv->string = g_strdup (text);

    g_signal_emit (findbar, signals[TEXT_CHANGED], 0);
  }
}

static gboolean
entry_key_press_event_cb (CtkWidget *entry,
                          CdkEventKey *event,
                          gpointer user_data)
{
  LogviewFindbar *findbar = user_data;

  if (event->keyval == CDK_KEY_Escape) {
    g_signal_emit (findbar, signals[CLOSE], 0);
    return TRUE;
  }

  return FALSE;
}

static gboolean
unbold_timeout_cb (gpointer user_data)
{
  LogviewFindbar *findbar = user_data;
  PangoFontDescription *desc;

  desc = pango_font_description_new ();
  ctk_widget_override_font (findbar->priv->message, desc);
  pango_font_description_free (desc);

  findbar->priv->status_bold_id = 0;

  return FALSE;
}

static void
logview_findbar_init (LogviewFindbar *findbar)
{
  CtkWidget *label, *w, *box;
  CtkToolbar *gtoolbar;
  CtkToolItem *item;
  LogviewFindbarPrivate *priv;

  priv = findbar->priv = logview_findbar_get_instance_private (findbar);

  gtoolbar = CTK_TOOLBAR (findbar);

  ctk_toolbar_set_style (gtoolbar, CTK_TOOLBAR_BOTH_HORIZ);

  priv->status_bold_id = 0;

  /* Find: |_______| */
  box = ctk_box_new (CTK_ORIENTATION_HORIZONTAL, 12);
  ctk_widget_set_halign (box, CTK_ALIGN_START);
  ctk_widget_set_margin_start (box, 2);
  ctk_widget_set_margin_end (box, 2);

  label = ctk_label_new_with_mnemonic (_("_Find:"));
  ctk_box_pack_start (CTK_BOX (box), label, FALSE, FALSE, 0);

  priv->entry = ctk_entry_new ();
  ctk_entry_set_width_chars (CTK_ENTRY (priv->entry), 32);
  ctk_label_set_mnemonic_widget (CTK_LABEL (label), priv->entry);
  ctk_box_pack_start (CTK_BOX (box), priv->entry, TRUE, TRUE, 0);

  item = ctk_tool_item_new ();
  ctk_container_add (CTK_CONTAINER (item), box);
  ctk_toolbar_insert (gtoolbar, item, -1);
  ctk_widget_show_all (CTK_WIDGET (item));

  /* "Previous" and "Next" buttons */
  w = ctk_image_new_from_icon_name ("pan-start-symbolic", CTK_ICON_SIZE_BUTTON);
  priv->back_button = ctk_tool_button_new (w, _("Find Previous"));
  ctk_tool_item_set_is_important (priv->back_button, TRUE);
  ctk_tool_item_set_tooltip_text (priv->back_button,
                                 _("Find previous occurrence of the search string"));
  ctk_toolbar_insert (gtoolbar, priv->back_button, -1);
  ctk_widget_show_all (CTK_WIDGET (priv->back_button));

  w = ctk_image_new_from_icon_name ("pan-end-symbolic", CTK_ICON_SIZE_BUTTON);
  priv->forward_button = ctk_tool_button_new (w, _("Find Next"));
  ctk_tool_item_set_is_important (priv->forward_button, TRUE);
  ctk_tool_item_set_tooltip_text (priv->forward_button,
                                 _("Find next occurrence of the search string"));
  ctk_toolbar_insert (gtoolbar, priv->forward_button, -1);
  ctk_widget_show_all (CTK_WIDGET (priv->forward_button));

  /* clear button */
  priv->clear_button = ctk_tool_button_new_from_stock (CTK_STOCK_CLEAR);
  ctk_tool_item_set_tooltip_text (priv->clear_button,
                                 _("Clear the search string"));
  ctk_toolbar_insert (gtoolbar, priv->clear_button, -1);
  ctk_widget_show_all (CTK_WIDGET (priv->clear_button));

  /* separator */
  priv->separator = ctk_separator_tool_item_new ();
  ctk_toolbar_insert (gtoolbar, priv->separator, -1);

  /* message */
  priv->status_item = ctk_tool_item_new ();
  ctk_tool_item_set_expand (priv->status_item, TRUE);
  priv->message = ctk_label_new ("");
  ctk_label_set_use_markup (CTK_LABEL (priv->message), TRUE);
  ctk_label_set_xalign (CTK_LABEL (priv->message), 0.0);
  ctk_label_set_yalign (CTK_LABEL (priv->message), 0.5);
  ctk_container_add (CTK_CONTAINER (priv->status_item), priv->message);
  ctk_widget_show (priv->message);
  ctk_toolbar_insert (gtoolbar, priv->status_item, -1);

  priv->string = NULL;

  /* signal handlers */
  g_signal_connect (priv->back_button, "clicked",
                    G_CALLBACK (back_button_clicked_cb), findbar);
  g_signal_connect (priv->forward_button, "clicked",
                    G_CALLBACK (forward_button_clicked_cb), findbar);
  g_signal_connect (priv->clear_button, "clicked",
                    G_CALLBACK (clear_button_clicked_cb), findbar);
  g_signal_connect (priv->entry, "activate",
                    G_CALLBACK (entry_activate_cb), findbar);
  g_signal_connect (priv->entry, "changed",
                    G_CALLBACK (entry_changed_cb), findbar);
  g_signal_connect (priv->entry, "key-press-event",
                    G_CALLBACK (entry_key_press_event_cb), findbar);
}

static void
do_grab_focus (CtkWidget *widget)
{
  LogviewFindbar *findbar = LOGVIEW_FINDBAR (widget);

  ctk_widget_grab_focus (findbar->priv->entry);
}

static void
do_finalize (GObject *obj)
{
  LogviewFindbar *findbar = LOGVIEW_FINDBAR (obj);

  g_free (findbar->priv->string);

  G_OBJECT_CLASS (logview_findbar_parent_class)->finalize (obj);
}

static void
logview_findbar_class_init (LogviewFindbarClass *klass)
{
  GObjectClass *oclass = G_OBJECT_CLASS (klass);
  CtkWidgetClass *wclass = CTK_WIDGET_CLASS (klass);

  oclass->finalize = do_finalize;

  wclass->grab_focus = do_grab_focus;

  signals[PREVIOUS] = g_signal_new ("previous",
                                    G_OBJECT_CLASS_TYPE (oclass),
                                    G_SIGNAL_RUN_LAST,
                                    G_STRUCT_OFFSET (LogviewFindbarClass, previous),
                                    NULL, NULL,
                                    g_cclosure_marshal_VOID__VOID,
                                    G_TYPE_NONE, 0);

  signals[NEXT] = g_signal_new ("next",
                                G_OBJECT_CLASS_TYPE (oclass),
                                G_SIGNAL_RUN_LAST,
                                G_STRUCT_OFFSET (LogviewFindbarClass, next),
                                NULL, NULL,
                                g_cclosure_marshal_VOID__VOID,
                                G_TYPE_NONE, 0);

  signals[CLOSE] = g_signal_new ("close",
                                 G_OBJECT_CLASS_TYPE (oclass),
                                 G_SIGNAL_RUN_LAST,
                                 G_STRUCT_OFFSET (LogviewFindbarClass, close),
                                 NULL, NULL,
                                 g_cclosure_marshal_VOID__VOID,
                                 G_TYPE_NONE, 0);

  signals[TEXT_CHANGED] = g_signal_new ("text-changed",
                                        G_OBJECT_CLASS_TYPE (oclass),
                                        G_SIGNAL_RUN_LAST,
                                        G_STRUCT_OFFSET (LogviewFindbarClass, text_changed),
                                        NULL, NULL,
                                        g_cclosure_marshal_VOID__VOID,
                                        G_TYPE_NONE, 0);
}

/* public methods */

CtkWidget *
logview_findbar_new (void)
{
  CtkWidget *widget;
  widget = g_object_new (LOGVIEW_TYPE_FINDBAR, NULL);
  return widget;
}

void
logview_findbar_open (LogviewFindbar *findbar)
{
  g_assert (LOGVIEW_IS_FINDBAR (findbar));

  ctk_widget_show (CTK_WIDGET (findbar));
  ctk_widget_grab_focus (CTK_WIDGET (findbar));
}

const char *
logview_findbar_get_text (LogviewFindbar *findbar)
{
  g_assert (LOGVIEW_IS_FINDBAR (findbar));

  return findbar->priv->string;
}

void
logview_findbar_set_message (LogviewFindbar *findbar,
                             const char *text)
{
  PangoFontDescription *desc;

  g_assert (LOGVIEW_IS_FINDBAR (findbar));

  if (text) {
    desc = pango_font_description_new ();
    pango_font_description_set_weight (desc, PANGO_WEIGHT_BOLD);
    ctk_widget_override_font (findbar->priv->message, desc);
    pango_font_description_free (desc);

    findbar->priv->status_bold_id = g_timeout_add (600, unbold_timeout_cb, findbar);
  }

  ctk_label_set_text (CTK_LABEL (findbar->priv->message),
                      text != NULL ? text : "");
  g_object_set (findbar->priv->separator, "visible", text != NULL, NULL);
  g_object_set (findbar->priv->status_item, "visible", text != NULL, NULL);
}
