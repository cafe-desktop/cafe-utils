/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * CAFE Search Tool
 *
 *  File:  gsearchtool-callbacks.h
 *
 *  (C) 2002 the Free Software Foundation
 *
 *  Authors:   	Dennis Cranston  <dennis_cranston@yahoo.com>
 *		George Lebl
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */

#ifndef _GSEARCHTOOL_CALLBACKS_H_
#define _GSEARCHTOOL_CALLBACKS_H_

#ifdef __cplusplus
extern "C" {
#pragma }
#endif

#include "eggsmclient.h"

void
version_cb (const gchar * option_name,
            const gchar * value,
            gpointer data,
            GError ** error);
void
quit_session_cb (EggSMClient * client,
                 gpointer data);
void
quit_cb (CtkWidget * widget,
         CdkEvent * event,
         gpointer data);
void
click_close_cb (CtkWidget * widget,
                gpointer data);
void
click_find_cb (CtkWidget * widget,
               gpointer	data);
void
click_stop_cb (CtkWidget * widget,
               gpointer	data);
void
click_help_cb (CtkWidget * widget,
               gpointer data);
void
click_expander_cb (GObject * object,
                   GParamSpec * param_spec,
                   gpointer data);
void
size_allocate_cb (CtkWidget * widget,
                  CtkAllocation * allocation,
                  gpointer data);
void
add_constraint_cb (CtkWidget * widget,
                   gpointer data);
void
remove_constraint_cb (CtkWidget * widget,
                      gpointer data);
void
constraint_activate_cb (CtkWidget * widget,
                        gpointer data);
void
constraint_update_info_cb (CtkWidget * widget,
                           gpointer data);
void
name_contains_activate_cb (CtkWidget * widget,
                           gpointer data);
void
look_in_folder_changed_cb (CtkWidget * widget,
                           gpointer data);
void
open_file_cb (CtkMenuItem * action,
              gpointer data);
void
open_file_event_cb (CtkWidget * widget,
                    CdkEventButton * event,
                    gpointer data);
void
open_folder_cb (CtkAction * action,
                gpointer data);
void
file_changed_cb (GFileMonitor * handle,
                 const gchar * monitor_uri,
                 const gchar * info_uri,
                 GFileMonitorEvent event_type,
                 gpointer data);
void
move_to_trash_cb (CtkAction * action,
                  gpointer data);
void
drag_begin_file_cb (CtkWidget * widget,
                    CdkDragContext * context,
                    gpointer data);
void
drag_file_cb (CtkWidget * widget,
              CdkDragContext * context,
              CtkSelectionData * selection_data,
              guint info,
              guint time,
              gpointer data);
void
show_file_selector_cb (CtkAction * action,
                       gpointer data);
void
save_results_cb (CtkWidget * chooser,
                 gint response,
                 gpointer data);
void
save_session_cb (EggSMClient * client,
                 GKeyFile * state_file,
                 gpointer client_data);
gboolean
key_press_cb (CtkWidget * widget,
              CdkEventKey * event,
              gpointer data);
gboolean
file_button_release_event_cb (CtkWidget * widget,
                              CdkEventButton * event,
                              gpointer data);
gboolean
file_event_after_cb (CtkWidget 	* widget,
                     CdkEventButton * event,
                     gpointer data);
gboolean
file_button_press_event_cb (CtkWidget * widget,
                            CdkEventButton * event,
                            gpointer data);
gboolean
file_key_press_event_cb (CtkWidget * widget,
                         CdkEventKey * event,
                         gpointer data);
gboolean
file_motion_notify_cb (CtkWidget *widget,
                       CdkEventMotion *event,
                       gpointer user_data);
gboolean
file_leave_notify_cb (CtkWidget *widget,
                      CdkEventCrossing *event,
                      gpointer user_data);
gboolean
not_running_timeout_cb (gpointer data);

void
disable_quick_search_cb (CtkWidget * dialog,
                         gint response,
                         gpointer data);
void
single_click_to_activate_key_changed_cb (GSettings * settings,
                                         gchar * key,
                                         gpointer user_data);
void
columns_changed_cb (CtkTreeView * treeview,
                    gpointer user_data);
gboolean
window_state_event_cb (CtkWidget * widget,
                       CdkEventWindowState * event,
                       gpointer data);

#ifdef __cplusplus
}
#endif

#endif /* _GSEARCHTOOL_CALLBACKS_H_ */
