/* screenshot-utils.c - common functions for CAFE Screenshot
 *
 * Copyright (C) 2001-2006  Jonathan Blandford <jrb@alum.mit.edu>
 * Copyright (C) 2008 Cosimo Cecchi <cosimoc@gnome.org>
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

#include "config.h"
#include "screenshot-utils.h"

#include <X11/Xatom.h>
#include <cdk/cdkx.h>
#include <cdk/cdkkeysyms.h>
#include <ctk/ctk.h>
#include <glib.h>
#include <glib/gi18n.h>

#ifdef HAVE_X11_EXTENSIONS_SHAPE_H
#include <X11/extensions/shape.h>
#endif

static CtkWidget *selection_window;

#define SELECTION_NAME "_CAFE_PANEL_SCREENSHOT"

/* To make sure there is only one screenshot taken at a time,
 * (Imagine key repeat for the print screen key) we hold a selection
 * until we are done taking the screenshot
 */
gboolean
screenshot_grab_lock (void)
{
  CdkAtom selection_atom;
  gboolean result = FALSE;
  CdkDisplay *display;

  selection_atom = cdk_atom_intern (SELECTION_NAME, FALSE);
  cdk_x11_grab_server ();

  if (cdk_selection_owner_get (selection_atom) != NULL)
    goto out;

  selection_window = ctk_invisible_new ();
  ctk_widget_show (selection_window);

  if (!ctk_selection_owner_set (selection_window,
				cdk_atom_intern (SELECTION_NAME, FALSE),
				CDK_CURRENT_TIME))
    {
      ctk_widget_destroy (selection_window);
      selection_window = NULL;
      goto out;
    }

  result = TRUE;

 out:
  cdk_x11_ungrab_server ();

  display = cdk_display_get_default ();
  cdk_display_flush (display);

  return result;
}

void
screenshot_release_lock (void)
{
  CdkDisplay *display;

  if (selection_window)
    {
      ctk_widget_destroy (selection_window);
      selection_window = NULL;
    }

  display = cdk_display_get_default ();
  cdk_display_flush (display);
}

static CdkWindow *
screen_get_active_window (CdkScreen *screen)
{
  CdkWindow *ret = NULL;
  Atom type_return;
  gint format_return;
  gulong nitems_return;
  gulong bytes_after_return;
  guchar *data = NULL;

  if (!cdk_x11_screen_supports_net_wm_hint (screen,
                                            cdk_atom_intern_static_string ("_NET_ACTIVE_WINDOW")))
    return NULL;

  if (XGetWindowProperty (CDK_DISPLAY_XDISPLAY (cdk_screen_get_display (screen)),
                          RootWindow (CDK_DISPLAY_XDISPLAY (cdk_screen_get_display (screen)),
                                      CDK_SCREEN_XNUMBER (screen)),
                          cdk_x11_get_xatom_by_name_for_display (cdk_screen_get_display (screen),
                                                                 "_NET_ACTIVE_WINDOW"),
                          0, 1, False, XA_WINDOW, &type_return,
                          &format_return, &nitems_return,
                          &bytes_after_return, &data)
      == Success)
    {
      if ((type_return == XA_WINDOW) && (format_return == 32) && (data))
        {
          Window window = *(Window *) data;

          if (window != None)
            {
              ret = cdk_x11_window_foreign_new_for_display (cdk_screen_get_display (screen),
                                                            window);
            }
        }
    }

  if (data)
    XFree (data);

  return ret;
}

static CdkWindow *
screenshot_find_active_window (void)
{
  CdkWindow *window;
  CdkScreen *default_screen;

  default_screen = cdk_screen_get_default ();
  window = screen_get_active_window (default_screen);

  return window;
}

static gboolean
screenshot_window_is_desktop (CdkWindow *window)
{
  CdkWindow *root_window = cdk_get_default_root_window ();
  CdkWindowTypeHint window_type_hint;

  if (window == root_window)
    return TRUE;

  window_type_hint = cdk_window_get_type_hint (window);
  if (window_type_hint == CDK_WINDOW_TYPE_HINT_DESKTOP)
    return TRUE;

  return FALSE;

}

CdkWindow *
screenshot_find_current_window ()
{
  CdkWindow *current_window;
  CdkDisplay *display;
  CdkSeat *seat;
  CdkDevice *device;

  current_window = screenshot_find_active_window ();
  display = cdk_window_get_display (current_window);
  seat = cdk_display_get_default_seat (display);
  device = cdk_seat_get_pointer (seat);

  /* If there's no active window, we fall back to returning the
   * window that the cursor is in.
   */
  if (!current_window)
    current_window = cdk_device_get_window_at_position (device, NULL, NULL);

  if (current_window)
    {
      if (screenshot_window_is_desktop (current_window))
	/* if the current window is the desktop (e.g. baul), we
	 * return NULL, as getting the whole screen makes more sense.
         */
        return NULL;

      /* Once we have a window, we take the toplevel ancestor. */
      current_window = cdk_window_get_toplevel (current_window);
    }

  return current_window;
}

typedef struct {
  CdkRectangle rect;
  gboolean button_pressed;
  CtkWidget *window;
} select_area_filter_data;

static gboolean
select_area_button_press (CtkWidget               *window,
                          CdkEventButton          *event,
			  select_area_filter_data *data)
{
  if (data->button_pressed)
    return TRUE;

  data->button_pressed = TRUE;
  data->rect.x = event->x_root;
  data->rect.y = event->y_root;

  return TRUE;
}

static gboolean
select_area_motion_notify (CtkWidget               *window,
                           CdkEventMotion          *event,
                           select_area_filter_data *data)
{
  CdkRectangle draw_rect;

  if (!data->button_pressed)
    return TRUE;

  draw_rect.width = ABS (data->rect.x - event->x_root);
  draw_rect.height = ABS (data->rect.y - event->y_root);
  draw_rect.x = MIN (data->rect.x, event->x_root);
  draw_rect.y = MIN (data->rect.y, event->y_root);

  if (draw_rect.width <= 0 || draw_rect.height <= 0)
    {
      ctk_window_move (CTK_WINDOW (window), -100, -100);
      ctk_window_resize (CTK_WINDOW (window), 10, 10);
      return TRUE;
    }

  ctk_window_move (CTK_WINDOW (window), draw_rect.x, draw_rect.y);
  ctk_window_resize (CTK_WINDOW (window), draw_rect.width, draw_rect.height);

  /* We (ab)use app-paintable to indicate if we have an RGBA window */
  if (!ctk_widget_get_app_paintable (window))
    {
      CdkWindow *cdkwindow = ctk_widget_get_window (window);

      /* Shape the window to make only the outline visible */
      if (draw_rect.width > 2 && draw_rect.height > 2)
        {
          cairo_region_t *region, *region2;
          cairo_rectangle_int_t region_rect = {
	    0, 0,
            draw_rect.width - 2, draw_rect.height - 2
          };

          region = cairo_region_create_rectangle (&region_rect);
          region_rect.x++;
          region_rect.y++;
          region_rect.width -= 2;
          region_rect.height -= 2;
          region2 = cairo_region_create_rectangle (&region_rect);
          cairo_region_subtract (region, region2);

          cdk_window_shape_combine_region (cdkwindow, region, 0, 0);

          cairo_region_destroy (region);
          cairo_region_destroy (region2);
        }
      else
        cdk_window_shape_combine_region (cdkwindow, NULL, 0, 0);
    }

  return TRUE;
}

static gboolean
select_area_button_release (CtkWidget *window,
                            CdkEventButton *event,
                            select_area_filter_data *data)
{
  if (!data->button_pressed)
    return TRUE;

  data->rect.width = ABS (data->rect.x - event->x_root);
  data->rect.height = ABS (data->rect.y - event->y_root);
  data->rect.x = MIN (data->rect.x, event->x_root);
  data->rect.y = MIN (data->rect.y, event->y_root);

  ctk_main_quit ();

  return TRUE;
}

static gboolean
select_area_key_press (CtkWidget *window,
                       CdkEventKey *event,
                       select_area_filter_data *data)
{
  if (event->keyval == CDK_KEY_Escape)
    {
      data->rect.x = 0;
      data->rect.y = 0;
      data->rect.width  = 0;
      data->rect.height = 0;
      ctk_main_quit ();
    }

  return TRUE;
}


static gboolean
draw (CtkWidget *window, cairo_t *cr, gpointer unused)
{
  CtkStyleContext *style;

  style = ctk_widget_get_style_context (window);

  if (ctk_widget_get_app_paintable (window))
    {
      cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
      cairo_set_source_rgba (cr, 0, 0, 0, 0);
      cairo_paint (cr);

      ctk_style_context_save (style);
      ctk_style_context_add_class (style, CTK_STYLE_CLASS_RUBBERBAND);

      ctk_render_background (style, cr,
                             0, 0,
                             ctk_widget_get_allocated_width (window),
                             ctk_widget_get_allocated_height (window));
      ctk_render_frame (style, cr,
                        0, 0,
                        ctk_widget_get_allocated_width (window),
                        ctk_widget_get_allocated_height (window));

      ctk_style_context_restore (style);
    }

  return TRUE;
}

static CtkWidget *
create_select_window (void)
{
  CdkScreen *screen = cdk_screen_get_default ();
  CtkWidget *window = ctk_window_new (CTK_WINDOW_POPUP);

  CdkVisual *visual = cdk_screen_get_rgba_visual (screen);
  if (cdk_screen_is_composited (screen) && visual)
    {
      ctk_widget_set_visual (window, visual);
      ctk_widget_set_app_paintable (window, TRUE);
    }

  g_signal_connect (window, "draw", G_CALLBACK (draw), NULL);

  ctk_window_move (CTK_WINDOW (window), -100, -100);
  ctk_window_resize (CTK_WINDOW (window), 10, 10);
  ctk_widget_show (window);
  return window;
}

typedef struct {
  CdkRectangle rectangle;
  SelectAreaCallback callback;
} CallbackData;

static gboolean
emit_select_callback_in_idle (gpointer user_data)
{
  CallbackData *data = user_data;

  data->callback (&data->rectangle);

  g_slice_free (CallbackData, data);

  return FALSE;
}

void
screenshot_select_area_async (SelectAreaCallback callback)
{
  CdkDisplay *display;
  CdkCursor               *cursor;
  CdkSeat *seat;
  CdkGrabStatus res;
  select_area_filter_data  data;
  CallbackData *cb_data;

  data.rect.x = 0;
  data.rect.y = 0;
  data.rect.width  = 0;
  data.rect.height = 0;
  data.button_pressed = FALSE;
  data.window = create_select_window();

  cb_data = g_slice_new0 (CallbackData);
  cb_data->callback = callback;

  g_signal_connect (data.window, "key-press-event", G_CALLBACK (select_area_key_press), &data);
  g_signal_connect (data.window, "button-press-event", G_CALLBACK (select_area_button_press), &data);
  g_signal_connect (data.window, "button-release-event", G_CALLBACK (select_area_button_release), &data);
  g_signal_connect (data.window, "motion-notify-event", G_CALLBACK (select_area_motion_notify), &data);

  display = cdk_display_get_default ();
  cursor = cdk_cursor_new_for_display (display, CDK_CROSSHAIR);

  seat = cdk_display_get_default_seat (display);

  res = cdk_seat_grab (seat,
                       ctk_widget_get_window (data.window),
                       CDK_SEAT_CAPABILITY_ALL,
                       FALSE,
                       cursor,
                       NULL,
                       NULL,
                       NULL);

  if (res != CDK_GRAB_SUCCESS)
    {
      g_object_unref (cursor);
      goto out;
    }

  ctk_main ();

  cdk_seat_ungrab (seat);

  ctk_widget_destroy (data.window);
  g_object_unref (cursor);
  cdk_display_flush (display);

out:
  cb_data->rectangle = data.rect;

  /* FIXME: we should actually be emitting the callback When
   * the compositor has finished re-drawing, but there seems to be no easy
   * way to know that.
   */
  g_timeout_add (200, emit_select_callback_in_idle, cb_data);
}

static Window
find_wm_window (Window xid)
{
  Window root, parent, *children;
  unsigned int nchildren;

  do
    {
      if (XQueryTree (CDK_DISPLAY_XDISPLAY (cdk_display_get_default ()), xid, &root,
		      &parent, &children, &nchildren) == 0)
	{
	  g_warning ("Couldn't find window manager window");
	  return None;
	}

      if (root == parent)
	return xid;

      xid = parent;
    }
  while (TRUE);
}

static cairo_region_t *
make_region_with_monitors (CdkScreen *screen)
{
  CdkDisplay     *display;
  cairo_region_t *region;
  int num_monitors;
  int i;

  display = cdk_screen_get_display (screen);
  num_monitors = cdk_display_get_n_monitors (display);

  region = cairo_region_create ();

  for (i = 0; i < num_monitors; i++)
    {
      CdkRectangle rect;

      cdk_monitor_get_geometry (cdk_display_get_monitor (display, i), &rect);
      cairo_region_union_rectangle (region, &rect);
    }

  return region;
}

static void
blank_rectangle_in_pixbuf (GdkPixbuf *pixbuf, CdkRectangle *rect)
{
  int x, y;
  int x2, y2;
  guchar *pixels;
  int rowstride;
  int n_channels;
  guchar *row;
  gboolean has_alpha;

  g_assert (gdk_pixbuf_get_colorspace (pixbuf) == GDK_COLORSPACE_RGB);

  x2 = rect->x + rect->width;
  y2 = rect->y + rect->height;

  pixels = gdk_pixbuf_get_pixels (pixbuf);
  rowstride = gdk_pixbuf_get_rowstride (pixbuf);
  has_alpha = gdk_pixbuf_get_has_alpha (pixbuf);
  n_channels = gdk_pixbuf_get_n_channels (pixbuf);

  for (y = rect->y; y < y2; y++)
    {
      guchar *p;

      row = pixels + y * rowstride;
      p = row + rect->x * n_channels;

      for (x = rect->x; x < x2; x++)
	{
	  *p++ = 0;
	  *p++ = 0;
	  *p++ = 0;

	  if (has_alpha)
	    *p++ = 255; /* opaque black */
	}
    }
}

static void
blank_region_in_pixbuf (GdkPixbuf *pixbuf, cairo_region_t *region)
{
  int n_rects;
  int i;
  int width, height;
  cairo_rectangle_int_t pixbuf_rect;

  n_rects = cairo_region_num_rectangles (region);

  width = gdk_pixbuf_get_width (pixbuf);
  height = gdk_pixbuf_get_height (pixbuf);

  pixbuf_rect.x	     = 0;
  pixbuf_rect.y	     = 0;
  pixbuf_rect.width  = width;
  pixbuf_rect.height = height;

  for (i = 0; i < n_rects; i++)
    {
      cairo_rectangle_int_t rect, dest;

      cairo_region_get_rectangle (region, i, &rect);
      if (cdk_rectangle_intersect (&rect, &pixbuf_rect, &dest))
	blank_rectangle_in_pixbuf (pixbuf, &dest);

    }
}

/* When there are multiple monitors with different resolutions, the visible area
 * within the root window may not be rectangular (it may have an L-shape, for
 * example).  In that case, mask out the areas of the root window which would
 * not be visible in the monitors, so that screenshot do not end up with content
 * that the user won't ever see.
 */
static void
mask_monitors (GdkPixbuf *pixbuf, CdkWindow *root_window)
{
  CdkScreen *screen;
  cairo_region_t *region_with_monitors;
  cairo_region_t *invisible_region;
  cairo_rectangle_int_t rect;
  gint scale;

  screen = cdk_window_get_screen (root_window);
  scale = cdk_window_get_scale_factor (root_window);

  region_with_monitors = make_region_with_monitors (screen);

  rect.x = 0;
  rect.y = 0;
  rect.width = WidthOfScreen (cdk_x11_screen_get_xscreen (screen)) / scale;
  rect.height = HeightOfScreen (cdk_x11_screen_get_xscreen (screen)) / scale;

  invisible_region = cairo_region_create_rectangle (&rect);
  cairo_region_subtract (invisible_region, region_with_monitors);

  blank_region_in_pixbuf (pixbuf, invisible_region);

  cairo_region_destroy (region_with_monitors);
  cairo_region_destroy (invisible_region);
}

GdkPixbuf *
screenshot_get_pixbuf (CdkWindow    *window,
                       CdkRectangle *rectangle,
                       gboolean      include_pointer,
                       gboolean      include_border,
                       gboolean      include_mask)
{
  CdkWindow *root;
  GdkPixbuf *screenshot;
  gint x_real_orig, y_real_orig, x_orig, y_orig;
  gint width, real_width, height, real_height;
  gint screen_width, screen_height, scale;

  /* If the screenshot should include the border, we look for the WM window. */

  if (include_border)
    {
      Window xid, wm;

      xid = CDK_WINDOW_XID (window);
      wm = find_wm_window (xid);

      if (wm != None)
        window = cdk_x11_window_foreign_new_for_display (cdk_display_get_default (), wm);

      /* fallback to no border if we can't find the WM window. */
    }

  root = cdk_get_default_root_window ();
  scale = cdk_window_get_scale_factor (root);

  real_width = cdk_window_get_width (window);
  real_height = cdk_window_get_height (window);

  screen_width = WidthOfScreen (cdk_x11_screen_get_xscreen (cdk_screen_get_default ())) / scale;
  screen_height = HeightOfScreen (cdk_x11_screen_get_xscreen (cdk_screen_get_default ())) / scale;

  cdk_window_get_origin (window, &x_real_orig, &y_real_orig);

  x_orig = x_real_orig;
  y_orig = y_real_orig;
  width  = real_width;
  height = real_height;

  if (x_orig < 0)
    {
      width = width + x_orig;
      x_orig = 0;
    }

  if (y_orig < 0)
    {
      height = height + y_orig;
      y_orig = 0;
    }

  if (x_orig + width > screen_width)
    width = screen_width - x_orig;

  if (y_orig + height > screen_height)
    height = screen_height - y_orig;

  if (rectangle)
    {
      x_orig = rectangle->x - x_orig;
      y_orig = rectangle->y - y_orig;
      width  = rectangle->width;
      height = rectangle->height;
    }

  screenshot = gdk_pixbuf_get_from_window (root,
                                           x_orig, y_orig,
                                           width, height);

  /*
   * Masking currently only works properly with full-screen shots
   */
  if (include_mask)
      mask_monitors (screenshot, root);

#ifdef HAVE_X11_EXTENSIONS_SHAPE_H
  if (include_border)
    {
      XRectangle *rectangles;
      GdkPixbuf *tmp;
      int rectangle_count, rectangle_order, i;

      /* we must use XShape to avoid showing what's under the rounder corners
       * of the WM decoration.
       */

      rectangles = XShapeGetRectangles (CDK_DISPLAY_XDISPLAY (cdk_display_get_default ()),
                                        CDK_WINDOW_XID (window),
                                        ShapeBounding,
                                        &rectangle_count,
                                        &rectangle_order);
      if (rectangles && rectangle_count > 0 && window != root)
        {
          gboolean has_alpha = gdk_pixbuf_get_has_alpha (screenshot);

          if (scale)
            {
              width *= scale;
              height *= scale;
            }

          tmp = gdk_pixbuf_new (GDK_COLORSPACE_RGB, TRUE, 8, width, height);
          gdk_pixbuf_fill (tmp, 0);

          for (i = 0; i < rectangle_count; i++)
            {
              gint rec_x, rec_y;
              gint rec_width, rec_height;
              gint y;

              rec_x = rectangles[i].x;
              rec_y = rectangles[i].y;
              rec_width = rectangles[i].width / scale;
              rec_height = rectangles[i].height / scale;

              if (x_real_orig < 0)
                {
                  rec_x += x_real_orig;
                  rec_x = MAX(rec_x, 0);
                  rec_width += x_real_orig;
                }

              if (y_real_orig < 0)
                {
                  rec_y += y_real_orig;
                  rec_y = MAX(rec_y, 0);
                  rec_height += y_real_orig;
                }

              if (x_orig + rec_x + rec_width > screen_width)
                rec_width = screen_width - x_orig - rec_x;

              if (y_orig + rec_y + rec_height > screen_height)
                rec_height = screen_height - y_orig - rec_y;

              if (scale)
                {
                  rec_width *= scale;
                  rec_height *= scale;
                }

              for (y = rec_y; y < rec_y + rec_height; y++)
                {
                  guchar *src_pixels, *dest_pixels;
                  gint x;

                  src_pixels = gdk_pixbuf_get_pixels (screenshot)
                             + y * gdk_pixbuf_get_rowstride (screenshot)
                             + rec_x * (has_alpha ? 4 : 3);
                  dest_pixels = gdk_pixbuf_get_pixels (tmp)
                              + y * gdk_pixbuf_get_rowstride (tmp)
                              + rec_x * 4;

                  for (x = 0; x < rec_width; x++)
                    {
                      *dest_pixels++ = *src_pixels++;
                      *dest_pixels++ = *src_pixels++;
                      *dest_pixels++ = *src_pixels++;

                      if (has_alpha)
                        *dest_pixels++ = *src_pixels++;
                      else
                        *dest_pixels++ = 255;
                    }
                }
            }

          g_object_unref (screenshot);
          screenshot = tmp;
        }
    }
#endif /* HAVE_X11_EXTENSIONS_SHAPE_H */

  /* if we have a selected area, there were by definition no cursor in the
   * screenshot */
  if (include_pointer && !rectangle)
    {
      CdkCursor *cursor;
      GdkPixbuf *cursor_pixbuf;

      cursor = cdk_cursor_new_for_display (cdk_display_get_default (), CDK_LEFT_PTR);
      cursor_pixbuf = cdk_cursor_get_image (cursor);

      if (cursor_pixbuf != NULL)
        {
          CdkDisplay *display;
          CdkSeat *seat;
          CdkDevice *device;
          CdkRectangle r1, r2;
          gint cx, cy, xhot, yhot;

          display = cdk_window_get_display (window);
          seat = cdk_display_get_default_seat (display);
          device = cdk_seat_get_pointer (seat);

          cdk_window_get_device_position (window, device, &cx, &cy, NULL);
          sscanf (gdk_pixbuf_get_option (cursor_pixbuf, "x_hot"), "%d", &xhot);
          sscanf (gdk_pixbuf_get_option (cursor_pixbuf, "y_hot"), "%d", &yhot);

          if (scale)
            {
              cx *= scale;
              cy *= scale;
            }

          /* in r1 we have the window coordinates */
          r1.x = x_real_orig;
          r1.y = y_real_orig;
          r1.width = real_width * scale;
          r1.height = real_height * scale;

          /* in r2 we have the cursor window coordinates */
          r2.x = cx + x_real_orig;
          r2.y = cy + y_real_orig;
          r2.width = gdk_pixbuf_get_width (cursor_pixbuf);
          r2.height = gdk_pixbuf_get_height (cursor_pixbuf);

          /* see if the pointer is inside the window */
          if (cdk_rectangle_intersect (&r1, &r2, &r2))
            {
              gdk_pixbuf_composite (cursor_pixbuf, screenshot,
                                    cx - xhot, cy - yhot,
                                    r2.width, r2.height,
                                    cx - xhot, cy - yhot,
                                    1.0, 1.0,
                                    GDK_INTERP_BILINEAR,
                                    255);
            }

          g_object_unref (cursor_pixbuf);
          g_object_unref (cursor);
        }
    }

  return screenshot;
}

void
screenshot_show_error_dialog (CtkWindow   *parent,
                              const gchar *message,
                              const gchar *detail)
{
  CtkWidget *dialog;

  g_return_if_fail ((parent == NULL) || (CTK_IS_WINDOW (parent)));
  g_return_if_fail (message != NULL);

  dialog = ctk_message_dialog_new (parent,
  				   CTK_DIALOG_DESTROY_WITH_PARENT,
  				   CTK_MESSAGE_ERROR,
  				   CTK_BUTTONS_OK,
  				   "%s", message);
  ctk_window_set_title (CTK_WINDOW (dialog), "");

  if (detail)
    ctk_message_dialog_format_secondary_text (CTK_MESSAGE_DIALOG (dialog),
  					      "%s", detail);

  if (parent && ctk_window_get_group (parent))
    ctk_window_group_add_window (ctk_window_get_group (parent), CTK_WINDOW (dialog));

  ctk_dialog_run (CTK_DIALOG (dialog));

  ctk_widget_destroy (dialog);
}

void
screenshot_show_gerror_dialog (CtkWindow   *parent,
                               const gchar *message,
                               GError      *error)
{
  g_return_if_fail (parent == NULL || CTK_IS_WINDOW (parent));
  g_return_if_fail (message != NULL);
  g_return_if_fail (error != NULL);

  screenshot_show_error_dialog (parent, message, error->message);
}
