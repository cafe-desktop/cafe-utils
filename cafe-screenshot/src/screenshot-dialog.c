/* screenshot-dialog.c - main CAFE Screenshot dialog
 *
 * Copyright (C) 2001-2006  Jonathan Blandford <jrb@alum.mit.edu>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 */

#include <config.h>
#include <string.h>
#include <stdlib.h>

#include "screenshot-dialog.h"
#include "screenshot-save.h"
#include <cdk/cdkkeysyms.h>
#include <glib/gi18n.h>
#include <gio/gio.h>

enum {
  TYPE_IMAGE_PNG,
  TYPE_TEXT_URI_LIST,

  LAST_TYPE
};

static CtkTargetEntry drag_types[] =
{
  { "image/png", 0, TYPE_IMAGE_PNG },
  { "text/uri-list", 0, TYPE_TEXT_URI_LIST },
};

struct ScreenshotDialog
{
  CtkBuilder *ui;
  GdkPixbuf *screenshot;
  GdkPixbuf *preview_image;
  CtkWidget *save_widget;
  CtkWidget *filename_entry;
  gint drag_x;
  gint drag_y;
};

static gboolean
on_toplevel_key_press_event (CtkWidget *widget,
			     CdkEventKey *key)
{
  if (key->keyval == CDK_KEY_F1)
    {
      ctk_dialog_response (CTK_DIALOG (widget), CTK_RESPONSE_HELP);
      return TRUE;
    }

  return FALSE;
}

static void
on_preview_draw (CtkWidget      *drawing_area,
                 cairo_t        *cr,
                 gpointer        data)
{
  ScreenshotDialog *dialog = data;
  CtkStyleContext *context;
  int width, height;

  width = ctk_widget_get_allocated_width (drawing_area);
  height = ctk_widget_get_allocated_height (drawing_area);

  if (!dialog->preview_image ||
      gdk_pixbuf_get_width (dialog->preview_image) != width ||
      gdk_pixbuf_get_height (dialog->preview_image) != height)
    {
      g_clear_object (&dialog->preview_image);
      dialog->preview_image = gdk_pixbuf_scale_simple (dialog->screenshot,
                                                       width,
                                                       height,
                                                       CDK_INTERP_BILINEAR);
    }

  context = ctk_widget_get_style_context (drawing_area);
  ctk_style_context_save (context);

  ctk_style_context_set_state (context, ctk_widget_get_state_flags (drawing_area));
  ctk_render_icon (context, cr, dialog->preview_image, 0, 0);

  ctk_style_context_restore (context);
}

static gboolean
on_preview_button_press_event (CtkWidget      *drawing_area,
			       CdkEventButton *event,
			       gpointer        data)
{
  ScreenshotDialog *dialog = data;

  dialog->drag_x = (int) event->x;
  dialog->drag_y = (int) event->y;

  return FALSE;
}

static gboolean
on_preview_button_release_event (CtkWidget      *drawing_area,
				 CdkEventButton *event,
				 gpointer        data)
{
  ScreenshotDialog *dialog = data;

  dialog->drag_x = 0;
  dialog->drag_y = 0;

  return FALSE;
}

static void
drag_data_get (CtkWidget          *widget,
	       CdkDragContext     *context,
	       CtkSelectionData   *selection_data,
	       guint               info,
	       guint               time,
	       ScreenshotDialog   *dialog)
{
  if (info == TYPE_TEXT_URI_LIST)
    {
      gchar **uris;

      uris = g_new (gchar *, 2);
      uris[0] = g_strconcat ("file://",
                             screenshot_save_get_filename (),
                             NULL);
      uris[1] = NULL;

      ctk_selection_data_set_uris (selection_data, uris);
      g_strfreev (uris);
    }
  else if (info == TYPE_IMAGE_PNG)
    {
      ctk_selection_data_set_pixbuf (selection_data, dialog->screenshot);
    }
  else
    {
      g_warning ("Unknown type %d", info);
    }
}

static void
drag_begin (CtkWidget        *widget,
	    CdkDragContext   *context,
	    ScreenshotDialog *dialog)
{
  ctk_drag_set_icon_pixbuf (context, dialog->preview_image,
			    dialog->drag_x, dialog->drag_y);
}


ScreenshotDialog *
screenshot_dialog_new (GdkPixbuf *screenshot,
		       char      *initial_uri,
		       gboolean   take_window_shot)
{
  ScreenshotDialog *dialog;
  CtkWidget *toplevel;
  CtkWidget *preview_darea;
  CtkWidget *aspect_frame;
  CtkWidget *file_chooser_box;
  gint width, height;
  char *current_folder;
  char *current_name;
  char *ext;
  gint pos;
  GFile *tmp_file;
  GFile *parent_file;
  GError *error = NULL;

  tmp_file = g_file_new_for_uri (initial_uri);
  parent_file = g_file_get_parent (tmp_file);

  current_name = g_file_get_basename (tmp_file);
  current_folder = g_file_get_uri (parent_file);
  g_object_unref (tmp_file);
  g_object_unref (parent_file);

  dialog = g_new0 (ScreenshotDialog, 1);

  dialog->ui = ctk_builder_new ();

  dialog->screenshot = screenshot;

  if (ctk_builder_add_from_resource (dialog->ui, "/org/cafe/screenshot/cafe-screenshot.ui", &error) == 0)
    {
      CtkWidget *dialog;
      dialog = ctk_message_dialog_new (NULL, 0,
				       CTK_MESSAGE_ERROR,
				       CTK_BUTTONS_OK,
				       _("Error loading UI definition file for the screenshot program: \n%s\n\n"
				         "Please check your installation of cafe-utils."), error->message);
      ctk_dialog_run (CTK_DIALOG (dialog));
      ctk_widget_destroy (dialog);

      g_error_free (error);
      exit (1);
    }

  ctk_builder_set_translation_domain (dialog->ui, GETTEXT_PACKAGE);

  width = gdk_pixbuf_get_width (screenshot);
  height = gdk_pixbuf_get_height (screenshot);

  width /= 5;
  height /= 5;

  toplevel = CTK_WIDGET (ctk_builder_get_object (dialog->ui, "toplevel"));
  aspect_frame = CTK_WIDGET (ctk_builder_get_object (dialog->ui, "aspect_frame"));
  preview_darea = CTK_WIDGET (ctk_builder_get_object (dialog->ui, "preview_darea"));
  dialog->filename_entry = CTK_WIDGET (ctk_builder_get_object (dialog->ui, "filename_entry"));
  file_chooser_box = CTK_WIDGET (ctk_builder_get_object (dialog->ui, "file_chooser_box"));

  dialog->save_widget = ctk_file_chooser_button_new (_("Select a folder"), CTK_FILE_CHOOSER_ACTION_SELECT_FOLDER);
  ctk_file_chooser_set_local_only (CTK_FILE_CHOOSER (dialog->save_widget), FALSE);
  ctk_file_chooser_set_current_folder_uri (CTK_FILE_CHOOSER (dialog->save_widget), current_folder);
  ctk_entry_set_text (CTK_ENTRY (dialog->filename_entry), current_name);

  ctk_box_pack_start (CTK_BOX (file_chooser_box), dialog->save_widget, TRUE, TRUE, 0);
  g_free (current_folder);

  ctk_widget_set_size_request (preview_darea, width, height);
  ctk_aspect_frame_set (CTK_ASPECT_FRAME (aspect_frame), 0.0, 0.5,
			gdk_pixbuf_get_width (screenshot)/
			(gfloat) gdk_pixbuf_get_height (screenshot),
			FALSE);
  g_signal_connect (toplevel, "key_press_event", G_CALLBACK (on_toplevel_key_press_event), dialog);
  g_signal_connect (preview_darea, "draw", G_CALLBACK (on_preview_draw), dialog);
  g_signal_connect (preview_darea, "button_press_event", G_CALLBACK (on_preview_button_press_event), dialog);
  g_signal_connect (preview_darea, "button_release_event", G_CALLBACK (on_preview_button_release_event), dialog);

  if (take_window_shot)
    ctk_frame_set_shadow_type (CTK_FRAME (aspect_frame), CTK_SHADOW_NONE);
  else
    ctk_frame_set_shadow_type (CTK_FRAME (aspect_frame), CTK_SHADOW_IN);

  /* setup dnd */
  g_signal_connect (G_OBJECT (preview_darea), "drag_begin",
		    G_CALLBACK (drag_begin), dialog);
  g_signal_connect (G_OBJECT (preview_darea), "drag_data_get",
		    G_CALLBACK (drag_data_get), dialog);

  ctk_widget_show_all (toplevel);

  /* select the name of the file but leave out the extension if there's any;
   * the dialog must be realized for select_region to work
   */
  ext = g_utf8_strrchr (current_name, -1, '.');
  if (ext)
    pos = g_utf8_strlen (current_name, -1) - g_utf8_strlen (ext, -1);
  else
    pos = -1;

  ctk_editable_select_region (CTK_EDITABLE (dialog->filename_entry),
			      0,
			      pos);

  g_free (current_name);

  return dialog;
}

void
screenshot_dialog_focus_entry (ScreenshotDialog *dialog)
{
  ctk_widget_grab_focus (dialog->filename_entry);
}

void
screenshot_dialog_enable_dnd (ScreenshotDialog *dialog)
{
  CtkWidget *preview_darea;

  g_return_if_fail (dialog != NULL);

  preview_darea = CTK_WIDGET (ctk_builder_get_object (dialog->ui, "preview_darea"));
  ctk_drag_source_set (preview_darea,
		       CDK_BUTTON1_MASK | CDK_BUTTON3_MASK,
		       drag_types, G_N_ELEMENTS (drag_types),
		       CDK_ACTION_COPY);
}

CtkWidget *
screenshot_dialog_get_toplevel (ScreenshotDialog *dialog)
{
  return CTK_WIDGET (ctk_builder_get_object (dialog->ui, "toplevel"));
}

char *
screenshot_dialog_get_uri (ScreenshotDialog *dialog)
{
  gchar *folder;
  const gchar *file_name;
  gchar *uri, *file, *tmp;
  GError *error;

  folder = ctk_file_chooser_get_uri (CTK_FILE_CHOOSER (dialog->save_widget));
  file_name = ctk_entry_get_text (CTK_ENTRY (dialog->filename_entry));

  error = NULL;
  tmp = g_filename_from_utf8 (file_name, -1, NULL, NULL, &error);
  if (error)
    {
      g_warning ("Unable to convert `%s' to valid UTF-8: %s\n"
                 "Falling back to default file.",
                 file_name,
                 error->message);
      g_error_free (error);
      tmp = g_strdup (_("Screenshot.png"));
    }

  file = g_uri_escape_string (tmp, NULL, FALSE);
  uri = g_build_filename (folder, file, NULL);

  g_free (folder);
  g_free (tmp);
  g_free (file);

  return uri;
}

char *
screenshot_dialog_get_folder (ScreenshotDialog *dialog)
{
  return ctk_file_chooser_get_uri (CTK_FILE_CHOOSER (dialog->save_widget));
}

GdkPixbuf *
screenshot_dialog_get_screenshot (ScreenshotDialog *dialog)
{
  return dialog->screenshot;
}

void
screenshot_dialog_set_busy (ScreenshotDialog *dialog,
			    gboolean          busy)
{
  CtkWidget *toplevel;
  CdkDisplay *display;

  toplevel = screenshot_dialog_get_toplevel (dialog);
  display = ctk_widget_get_display (toplevel);

  if (busy)
    {
      CdkCursor *cursor;
      /* Change cursor to busy */
      cursor = cdk_cursor_new_for_display (display, CDK_WATCH);
      cdk_window_set_cursor (ctk_widget_get_window (toplevel), cursor);
      g_object_unref (cursor);
    }
  else
    {
      cdk_window_set_cursor (ctk_widget_get_window (toplevel), NULL);
    }

  ctk_widget_set_sensitive (toplevel, ! busy);

  cdk_display_flush (display);
}
