/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * CAFE Search Tool
 *
 *  File:  gsearchtool.h
 *
 *  (C) 1998,2002 the Free Software Foundation
 *
 *  Authors:	Dennis Cranston  <dennis_cranston@yahoo.com>
 *              George Lebl
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

#ifndef _GSEARCHTOOL_H_
#define _GSEARCHTOOL_H_

#ifdef __cplusplus
extern "C" {
#pragma }
#endif

#include <ctk/ctk.h>

#define GSEARCH_TYPE_WINDOW gsearch_window_get_type()
#define GSEARCH_WINDOW(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), GSEARCH_TYPE_WINDOW, GSearchWindow))
#define GSEARCH_WINDOW_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), GSEARCH_TYPE_WINDOW, GSearchWindowClass))
#define GSEARCH_IS_WINDOW(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GSEARCH_TYPE_WINDOW))
#define GSEARCH_IS_WINDOW_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), GSEARCH_TYPE_WINDOW))
#define GSEARCH_WINDOW_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), GSEARCH_TYPE_WINDOW, GSearchWindowClass))

#define CAFE_SEARCH_TOOL_ICON "system-search"
#define MINIMUM_WINDOW_WIDTH   420
#define MINIMUM_WINDOW_HEIGHT  310
#define DEFAULT_WINDOW_WIDTH   554
#define DEFAULT_WINDOW_HEIGHT  350
#define WINDOW_HEIGHT_STEP      35
#define NUM_VISIBLE_COLUMNS      5
#define BAUL_PREFERENCES_SCHEMA "org.cafe.baul.preferences"

typedef enum {
	STOPPED,
	ABORTED,
	RUNNING,
	MAKE_IT_STOP,
	MAKE_IT_QUIT
} GSearchCommandStatus;

typedef enum {
	SPEED_TRADEOFF_ALWAYS = 0,
	SPEED_TRADEOFF_LOCAL_ONLY,
	SPEED_TRADEOFF_NEVER
} BaulSpeedTradeoff;

typedef enum {
	BAUL_DATE_FORMAT_LOCALE = 0,
	BAUL_DATE_FORMAT_ISO,
	BAUL_DATE_FORMAT_INFORMAL
} BaulDateFormat;

typedef enum {
	COLUMN_ICON,
	COLUMN_NAME,
	COLUMN_RELATIVE_PATH,
	COLUMN_LOCALE_FILE,
	COLUMN_READABLE_SIZE,
	COLUMN_SIZE,
	COLUMN_TYPE,
	COLUMN_READABLE_DATE,
	COLUMN_DATE,
	COLUMN_MONITOR,
	COLUMN_NO_FILES_FOUND,
	NUM_COLUMNS
} GSearchResultColumns;

typedef struct _GSearchWindow GSearchWindow;
typedef struct _GSearchWindowClass GSearchWindowClass;
typedef struct _GSearchCommandDetails GSearchCommandDetails;
typedef struct _GSearchConstraint GSearchConstraint;
typedef struct _GSearchMonitor GSearchMonitor;

struct _GSearchWindow {
	CtkWindow               parent_instance;

	CtkWidget             * window;
	CtkUIManager          * window_ui_manager;
	CdkGeometry             window_geometry;
	gint                    window_width;
	gint                    window_height;
	gboolean                is_window_maximized;
	gboolean                is_window_accessible;

	CtkWidget             * name_contains_entry;
	CtkWidget             * look_in_folder_button;
	CtkWidget             * name_and_folder_table;
	CtkWidget             * progress_spinner;
	CtkWidget             * find_button;
	CtkWidget             * stop_button;
	CtkWidget             * focus;

	CtkWidget             * show_more_options_expander;
	CtkWidget             * available_options_vbox;
	CtkWidget             * available_options_label;
	CtkWidget             * available_options_combo_box;
	CtkWidget             * available_options_add_button;
	CtkSizeGroup          * available_options_button_size_group;
	GList                 * available_options_selected_list;

	CtkWidget             * files_found_label;
	CtkWidget             * search_results_vbox;
	CtkWidget             * search_results_popup_menu;
	CtkWidget             * search_results_popup_submenu;
	CtkWidget             * search_results_save_results_as_item;
	CtkTreeView           * search_results_tree_view;
	CtkTreeViewColumn     * search_results_folder_column;
	CtkTreeViewColumn     * search_results_size_column;
	CtkTreeViewColumn     * search_results_type_column;
	CtkTreeViewColumn     * search_results_date_column;
	CtkListStore          * search_results_list_store;
	CtkCellRenderer       * search_results_name_cell_renderer;
	CtkTreeSelection      * search_results_selection;
	CtkTreeIter             search_results_iter;
	CtkTreePath           * search_results_hover_path;
	GHashTable            * search_results_filename_hash_table;
	GHashTable            * search_results_pixbuf_hash_table;
	BaulDateFormat          search_results_date_format;
	gint		        show_thumbnails_file_size_limit;
	gboolean		show_thumbnails;
	gboolean                is_search_results_single_click_to_activate;
	gboolean		is_locate_database_check_finished;
	gboolean		is_locate_database_available;

	gchar                 * save_results_as_default_filename;

	GSettings             * cafe_search_tool_settings;
	GSettings             * cafe_search_tool_select_settings;
	GSettings             * cafe_desktop_interface_settings;
	GSettings             * baul_settings;
	gboolean                baul_schema_exists;

	GSearchCommandDetails * command_details;
};

struct _GSearchCommandDetails {
	pid_t                   command_pid;
	GSearchCommandStatus    command_status;

	gchar                 * name_contains_pattern_string;
	gchar                 * name_contains_regex_string;
	gchar                 * look_in_folder_string;

	gboolean		is_command_first_pass;
	gboolean		is_command_using_quick_mode;
	gboolean		is_command_second_pass_enabled;
	gboolean		is_command_show_hidden_files_enabled;
	gboolean		is_command_regex_matching_enabled;
	gboolean		is_command_timeout_enabled;
};

struct _GSearchConstraint {
	gint constraint_id;
	union {
		gchar * text;
		gint 	time;
		gint 	number;
	} data;
};

struct _GSearchWindowClass {
	CtkWindowClass parent_class;
};

struct _GSearchMonitor {
	GSearchWindow         * gsearch;
	CtkTreeRowReference   * reference;
	GFileMonitor          * handle;
};

GType
gsearch_window_get_type (void);

gchar *
build_search_command (GSearchWindow * gsearch,
                      gboolean first_pass);
void
spawn_search_command (GSearchWindow * gsearch,
                      gchar * command);
void
add_constraint (GSearchWindow * gsearch,
                gint constraint_id,
                gchar * value,
                gboolean show_constraint);
void
update_constraint_info (GSearchConstraint * constraint,
                        gchar * info);
void
remove_constraint (gint constraint_id);

void
set_constraint_gsettings_boolean (gint constraint_id,
                                  gboolean flag);
void
set_constraint_selected_state (GSearchWindow * gsearch,
                               gint constraint_id,
                               gboolean state);
void
set_clone_command (GSearchWindow * gsearch,
                   gint * argcp,
                   gchar *** argvp,
                   gpointer client_data,
                   gboolean escape_values);
void
update_search_counts (GSearchWindow * gsearch);

gboolean
tree_model_iter_free_monitor (CtkTreeModel * model,
                              CtkTreePath * path,
                              CtkTreeIter * iter,
                              gpointer data);

#ifdef __cplusplus
}
#endif

#endif /* _GSEARCHTOOL_H_ */
