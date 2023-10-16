/*
 * baobab-chart.h
 *
 * Copyright (C) 2006, 2007, 2008 Igalia
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
 * Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301  USA
 *
 * Authors:
 *   Felipe Erias <femorandeira@igalia.com>
 *   Pablo Santamaria <psantamaria@igalia.com>
 *   Jacobo Aragunde <jaragunde@igalia.com>
 *   Eduardo Lima <elima@igalia.com>
 *   Mario Sanchez <msanchez@igalia.com>
 *   Miguel Gomez <magomez@igalia.com>
 *   Henrique Ferreiro <hferreiro@igalia.com>
 *   Alejandro Pinheiro <apinheiro@igalia.com>
 *   Carlos Sanmartin <csanmartin@igalia.com>
 *   Alejandro Garcia <alex@igalia.com>
 */

#ifndef __BAOBAB_CHART_H__
#define __BAOBAB_CHART_H__

#include <ctk/ctk.h>

G_BEGIN_DECLS

#define BAOBAB_CHART_TYPE       (baobab_chart_get_type ())
#define BAOBAB_CHART(obj)       (G_TYPE_CHECK_INSTANCE_CAST ((obj), BAOBAB_CHART_TYPE, BaobabChart))
#define BAOBAB_CHART_CLASS(obj) (G_TYPE_CHECK_CLASS_CAST ((obj), BAOBAB_CHART_TYPE, BaobabChartClass))
#define BAOBAB_IS_CHART(obj)    (G_TYPE_CHECK_INSTANCE_TYPE ((obj), BAOBAB_CHART_TYPE))
#define BAOBAB_IS_CHART_CLASS(obj)  (G_TYPE_CHECK_CLASS_TYPE ((obj), BAOBAB_CHART_TYPE))
#define BAOBAB_CHART_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), BAOBAB_CHART_TYPE, BaobabChartClass))

typedef struct _BaobabChart BaobabChart;
typedef struct _BaobabChartClass BaobabChartClass;
typedef struct _BaobabChartPrivate BaobabChartPrivate;
typedef struct _BaobabChartColor BaobabChartColor;
typedef struct _BaobabChartItem BaobabChartItem;

struct _BaobabChart
{
  CtkWidget parent;

  /* < private > */
  BaobabChartPrivate *priv;
};

struct _BaobabChartColor
{
  gdouble red;
  gdouble green;
  gdouble blue;
};

struct _BaobabChartItem
{
  gchar *name;
  gchar *size;
  guint depth;
  gdouble rel_start;
  gdouble rel_size;
  CtkTreeIter iter;
  gboolean visible;
  gboolean has_any_child;
  gboolean has_visible_children;
  GdkRectangle rect;

  GList *parent;

  gpointer data;
};

struct _BaobabChartClass
{
  CtkWidgetClass parent_class;

  /* Signal prototypes */
  void (* item_activated) (BaobabChart *chart,
                             CtkTreeIter *iter);

  /* Abstract methods */
  void (* draw_item) (CtkWidget *chart,
                      cairo_t *cr,
                      BaobabChartItem *item,
                      gboolean highlighted);

  void (* pre_draw) (CtkWidget *chart,
                     cairo_t *cr);

  void (* post_draw) (CtkWidget *chart,
                      cairo_t *cr);

  void (* calculate_item_geometry) (CtkWidget *chart,
                                    BaobabChartItem *item);

  gboolean (* is_point_over_item) (CtkWidget *chart,
                                   BaobabChartItem *item,
                                   gdouble x,
                                   gdouble y);

  void (* get_item_rectangle) (CtkWidget *chart,
                               BaobabChartItem *item);

  guint (* can_zoom_in) (CtkWidget *chart);
  guint (* can_zoom_out) (CtkWidget *chart);
};

GType baobab_chart_get_type (void) G_GNUC_CONST;
CtkWidget* baobab_chart_new (void);
void baobab_chart_set_model_with_columns (CtkWidget *chart,
                                          CtkTreeModel *model,
                                          guint name_column,
                                          guint size_column,
                                          guint info_column,
                                          guint percentage_column,
                                          guint valid_column,
                                          CtkTreePath *root);
void baobab_chart_set_model (CtkWidget *chart,
                             CtkTreeModel *model);
CtkTreeModel* baobab_chart_get_model (CtkWidget *chart);
void baobab_chart_set_max_depth (CtkWidget *chart,
                                 guint max_depth);
guint baobab_chart_get_max_depth (CtkWidget *chart);
void baobab_chart_set_root (CtkWidget *chart,
                            CtkTreePath *root);
CtkTreePath *baobab_chart_get_root (CtkWidget *chart);
void baobab_chart_freeze_updates (CtkWidget *chart);
void baobab_chart_thaw_updates (CtkWidget *chart);
void baobab_chart_get_item_color (BaobabChartColor *color,
                                  gdouble position,
                                  gint depth,
                                  gboolean highlighted);
void  baobab_chart_move_up_root (CtkWidget *chart);
void baobab_chart_zoom_in (CtkWidget *chart);
void baobab_chart_zoom_out (CtkWidget *chart);
void baobab_chart_save_snapshot (CtkWidget *chart);
gboolean baobab_chart_is_frozen (CtkWidget *chart);
BaobabChartItem *baobab_chart_get_highlighted_item (CtkWidget *chart);

gboolean baobab_chart_can_zoom_in (CtkWidget *chart);
gboolean baobab_chart_can_zoom_out (CtkWidget *chart);

G_END_DECLS

#endif
