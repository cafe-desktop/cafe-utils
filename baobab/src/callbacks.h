/*
 * callbacks.h
 * This file is part of baobab
 *
 * Copyright (C) 2005-2006 Fabio Marzocca  <thesaltydog@gmail.com>
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
 */

#ifndef __BAOBAB_CALLBACKS_H__
#define __BAOBAB_CALLBACKS_H__

#include <ctk/ctk.h>
#include "baobab-chart.h"

void on_quit_activate (CtkMenuItem *menuitem, gpointer user_data);
void on_about_activate (CtkMenuItem *menuitem, gpointer user_data);
void on_menuscanhome_activate (CtkMenuItem *menuitem, gpointer user_data);
void on_menuallfs_activate (CtkMenuItem *menuitem, gpointer user_data);
void on_menuscandir_activate (CtkMenuItem *menuitem, gpointer user_data);
void on_menu_stop_activate (CtkMenuItem *menuitem, gpointer user_data);
void on_menu_rescan_activate (CtkMenuItem *menuitem, gpointer user_data);
void on_tbscandir_clicked (CtkToolButton *toolbutton, gpointer user_data);
void on_tbscanhome_clicked (CtkToolButton *toolbutton, gpointer user_data);
void on_tbscanall_clicked (CtkToolButton *toolbutton, gpointer user_data);
void on_tbstop_clicked (CtkToolButton *toolbutton, gpointer user_data);
void on_tbrescan_clicked (CtkToolButton *toolbutton, gpointer user_data);
void on_radio_allfs_clicked (CtkButton *button, gpointer user_data);
void on_radio_dir_clicked (CtkButton *button, gpointer user_data);
gboolean on_delete_activate (CtkWidget *widget, CdkEvent *event, gpointer user_data);
void open_file_cb (CtkMenuItem *pmenu, gpointer dummy);
void scan_folder_cb (CtkMenuItem *pmenu, gpointer dummy);
void trash_dir_cb (CtkMenuItem *pmenu, gpointer dummy);
void list_all_cb (CtkMenuItem *pmenu, gpointer dummy);
void on_pref_menu (CtkAction *a, gpointer user_data);
void on_tb_scan_remote_clicked (CtkToolButton *toolbutton, gpointer user_data);
void on_menu_scan_rem_activate (CtkMenuItem *menuitem, gpointer user_data);
void on_ck_allocated_activate (CtkToggleAction *action, gpointer user_data);
void on_helpcontents_activate (CtkAction *a, gpointer user_data);
void on_tv_selection_changed (CtkTreeSelection *selection, gpointer user_data);
void on_move_upwards_cb (CtkCheckMenuItem *checkmenuitem, gpointer user_data);
void on_zoom_in_cb (CtkCheckMenuItem *checkmenuitem, gpointer user_data);
void on_zoom_out_cb (CtkCheckMenuItem *checkmenuitem, gpointer user_data);
void on_chart_snapshot_cb (CtkCheckMenuItem *checkmenuitem, gpointer user_data);

#endif /* __BAOBAB_CALLBACKS_H__ */
