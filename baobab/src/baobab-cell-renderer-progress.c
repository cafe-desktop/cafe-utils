/* baobab-cell-renderer-progress.c
 *
 * Copyright (C) 2006 Paolo Borelli
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
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include <stdlib.h>

#include "baobab-cell-renderer-progress.h"

enum
{
  PROP_0,
  PROP_PERC
};

struct _BaobabCellRendererProgressPrivate
{
  double perc;
};

G_DEFINE_TYPE_WITH_PRIVATE (BaobabCellRendererProgress, baobab_cell_renderer_progress, CTK_TYPE_CELL_RENDERER)

static void
baobab_cell_renderer_progress_init (BaobabCellRendererProgress *cellprogress)
{
  cellprogress->priv = baobab_cell_renderer_progress_get_instance_private (cellprogress);
  cellprogress->priv->perc = 0;

  ctk_cell_renderer_set_padding (CTK_CELL_RENDERER (cellprogress), 4, 4);
}

CtkCellRenderer*
baobab_cell_renderer_progress_new (void)
{
  return g_object_new (BAOBAB_TYPE_CELL_RENDERER_PROGRESS, NULL);
}

static void
baobab_cell_renderer_progress_get_property (GObject *object,
					    guint param_id,
					    GValue *value,
					    GParamSpec *pspec)
{
  BaobabCellRendererProgress *cellprogress = BAOBAB_CELL_RENDERER_PROGRESS (object);

  switch (param_id)
    {
    case PROP_PERC:
      g_value_set_double (value, cellprogress->priv->perc);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
    }
}

static void
baobab_cell_renderer_progress_set_property (GObject *object,
					    guint param_id,
					    const GValue *value,
					    GParamSpec   *pspec)
{
  BaobabCellRendererProgress *cellprogress = BAOBAB_CELL_RENDERER_PROGRESS (object);

  switch (param_id)
    {
    case PROP_PERC:
      cellprogress->priv->perc = g_value_get_double (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
    }
}

/* we simply have a fixed size */

#define FIXED_WIDTH   70
#define FIXED_HEIGHT  8

static void
baobab_cell_renderer_progress_get_size (CtkCellRenderer *cell,
					CtkWidget       *widget,
					const CdkRectangle *cell_area,
					gint            *x_offset,
					gint            *y_offset,
					gint            *width,
					gint            *height)
{
  gint calc_width;
  gint calc_height;
  gint xpad;
  gint ypad;
  gfloat xalign;
  gfloat yalign;

  ctk_cell_renderer_get_padding (cell, &xpad, &ypad);
  calc_width  = (gint) xpad * 2 + FIXED_WIDTH;
  calc_height = (gint) ypad * 2 + FIXED_HEIGHT;

  if (width)
    *width = calc_width;

  if (height)
    *height = calc_height;

  if (cell_area)
  {
    ctk_cell_renderer_get_alignment (cell, &xalign, &yalign);
    if (x_offset)
    {
      *x_offset = xalign * (cell_area->width - calc_width);
      *x_offset = MAX (*x_offset, 0);
    }

    if (y_offset)
    {
      *y_offset = yalign * (cell_area->height - calc_height);
      *y_offset = MAX (*y_offset, 0);
    }
  }
}

static void
set_color_according_to_perc (cairo_t *cr, double value)
{
  static CdkRGBA red;
  static CdkRGBA yellow;
  static CdkRGBA green;
  static gboolean colors_initialized = FALSE;

  if (!colors_initialized)
    {
      /* hardcoded tango colors */
      cdk_rgba_parse (&red, "#cc0000");
      cdk_rgba_parse (&yellow, "#edd400");
      cdk_rgba_parse (&green, "#73d216");

      colors_initialized = TRUE;
    }

  if (value <= 0)
    {
      cairo_set_source_rgb (cr, 1, 1, 1);
      return;
    }
  else if (value <= 33.33)
    {
      cdk_cairo_set_source_rgba (cr, &green);
      return;
    }
  else if (value <= 66.66)
    {
      cdk_cairo_set_source_rgba (cr, &yellow);
      return;
    }
  else if (value <= 100.0)
    {
      cdk_cairo_set_source_rgba (cr, &red);
      return;
    }
  else
    g_assert_not_reached ();
}

static void
baobab_cell_renderer_progress_render (CtkCellRenderer *cell,
				      cairo_t         *cr,
				      CtkWidget       *widget,
				      const CdkRectangle *background_area,
				      const CdkRectangle  *cell_area,
				      guint            flags)
{
  BaobabCellRendererProgress *cellprogress = BAOBAB_CELL_RENDERER_PROGRESS (cell);
  gint x, y, w, h, perc_w;
  gboolean is_rtl;
  gint xpad;
  gint ypad;
  CtkStyle *style;

  is_rtl = ctk_widget_get_direction (widget) == CTK_TEXT_DIR_RTL;

  ctk_cell_renderer_get_padding (cell, &xpad, &ypad);

  x = cell_area->x + xpad;
  y = cell_area->y + ypad;

  w = cell_area->width - xpad * 2;
  h = cell_area->height - ypad * 2;

  /*
   * we always use a white bar with black
   * border and green/yellow/red progress...
   * I know it's not theme friendly, but we don't
   * want a plain progress bar
   */

  cairo_rectangle (cr, x, y, w, h);
  cairo_set_source_rgb (cr, 0, 0, 0);
  cairo_fill (cr);

  style = ctk_widget_get_style (widget);
  x += style->xthickness;
  y += style->ythickness;
  w -= style->xthickness * 2;
  h -= style->ythickness * 2;

  cairo_rectangle (cr, x, y, w, h);
  cairo_set_source_rgb (cr, 1, 1, 1);
  cairo_fill (cr);

  perc_w = w * MAX (0, cellprogress->priv->perc) / 100;

  cairo_rectangle (cr, is_rtl ? (x + w - perc_w) : x, y, perc_w, h);
  set_color_according_to_perc (cr, cellprogress->priv->perc);
  cairo_fill (cr);
}

static void
baobab_cell_renderer_progress_class_init (BaobabCellRendererProgressClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  CtkCellRendererClass *cell_class = CTK_CELL_RENDERER_CLASS (klass);

  object_class->get_property = baobab_cell_renderer_progress_get_property;
  object_class->set_property = baobab_cell_renderer_progress_set_property;

  cell_class->get_size = baobab_cell_renderer_progress_get_size;
  cell_class->render = baobab_cell_renderer_progress_render;

  g_object_class_install_property (object_class,
				   PROP_PERC,
				   g_param_spec_double ("perc",
						        "percentage",
						        "precentage",
						        -1, 100, 0,
						        G_PARAM_READWRITE));
}
