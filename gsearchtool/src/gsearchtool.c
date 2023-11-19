/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * CAFE Search Tool
 *
 *  File:  gsearchtool.c
 *
 *  (C) 1998,2002 the Free Software Foundation
 *
 *  Authors:    Dennis Cranston  <dennis_cranston@yahoo.com>
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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <fnmatch.h>
#ifndef FNM_CASEFOLD
#  define FNM_CASEFOLD 0
#endif

#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <glib.h>
#include <glib/gi18n.h>
#include <cdk/cdk.h>
#include <gio/gio.h>
#include <locale.h>

#include "gsearchtool.h"
#include "gsearchtool-callbacks.h"
#include "gsearchtool-support.h"
#include "gsearchtool-entry.h"

#define CAFE_SEARCH_TOOL_DEFAULT_ICON_SIZE 16
#define CAFE_SEARCH_TOOL_STOCK "panel-searchtool"
#define CAFE_SEARCH_TOOL_REFRESH_DURATION  50000
#define LEFT_LABEL_SPACING "     "

static GObjectClass * parent_class;

typedef enum {
	SEARCH_CONSTRAINT_TYPE_BOOLEAN,
	SEARCH_CONSTRAINT_TYPE_NUMERIC,
	SEARCH_CONSTRAINT_TYPE_TEXT,
	SEARCH_CONSTRAINT_TYPE_DATE_BEFORE,
	SEARCH_CONSTRAINT_TYPE_DATE_AFTER,
	SEARCH_CONSTRAINT_TYPE_SEPARATOR,
	SEARCH_CONSTRAINT_TYPE_NONE
} GSearchConstraintType;

typedef struct _GSearchOptionTemplate GSearchOptionTemplate;

struct _GSearchOptionTemplate {
	GSearchConstraintType type; /* The available option type */
	gchar * option;             /* An option string to pass to the command */
	gchar * desc;               /* The description for display */
	gchar * units;              /* Optional units for display */
	gboolean is_selected;
};

static GSearchOptionTemplate GSearchOptionTemplates[] = {
	{ SEARCH_CONSTRAINT_TYPE_TEXT, NULL, N_("Contains the _text"), NULL, FALSE },
	{ SEARCH_CONSTRAINT_TYPE_SEPARATOR, NULL, NULL, NULL, TRUE },
	{ SEARCH_CONSTRAINT_TYPE_DATE_BEFORE, "-mtime -%d", N_("_Date modified less than"), N_("days"), FALSE },
	{ SEARCH_CONSTRAINT_TYPE_DATE_AFTER, "\\( -mtime +%d -o -mtime %d \\)", N_("Date modified more than"), N_("days"), FALSE },
	{ SEARCH_CONSTRAINT_TYPE_SEPARATOR, NULL, NULL, NULL, TRUE },
	{ SEARCH_CONSTRAINT_TYPE_NUMERIC, "\\( -size %uc -o -size +%uc \\)", N_("S_ize at least"), N_("kilobytes"), FALSE },
	{ SEARCH_CONSTRAINT_TYPE_NUMERIC, "\\( -size %uc -o -size -%uc \\)", N_("Si_ze at most"), N_("kilobytes"), FALSE },
	{ SEARCH_CONSTRAINT_TYPE_BOOLEAN, "-size 0c \\( -type f -o -type d \\)", N_("File is empty"), NULL, FALSE },
	{ SEARCH_CONSTRAINT_TYPE_SEPARATOR, NULL, NULL, NULL, TRUE },
	{ SEARCH_CONSTRAINT_TYPE_TEXT, "-user '%s'", N_("Owned by _user"), NULL, FALSE },
	{ SEARCH_CONSTRAINT_TYPE_TEXT, "-group '%s'", N_("Owned by _group"), NULL, FALSE },
	{ SEARCH_CONSTRAINT_TYPE_BOOLEAN, "\\( -nouser -o -nogroup \\)", N_("Owner is unrecognized"), NULL, FALSE },
	{ SEARCH_CONSTRAINT_TYPE_SEPARATOR, NULL, NULL, NULL, TRUE },
	{ SEARCH_CONSTRAINT_TYPE_TEXT, "'!' -name '*%s*'", N_("Na_me does not contain"), NULL, FALSE },
	{ SEARCH_CONSTRAINT_TYPE_TEXT, "-regex '%s'", N_("Name matches regular e_xpression"), NULL, FALSE },
	{ SEARCH_CONSTRAINT_TYPE_SEPARATOR, NULL, NULL, NULL, TRUE },
	{ SEARCH_CONSTRAINT_TYPE_BOOLEAN, "SHOW_HIDDEN_FILES", N_("Show hidden and backup files"), NULL, FALSE },
	{ SEARCH_CONSTRAINT_TYPE_BOOLEAN, "-follow", N_("Follow symbolic links"), NULL, FALSE },
	{ SEARCH_CONSTRAINT_TYPE_BOOLEAN, "EXCLUDE_OTHER_FILESYSTEMS", N_("Exclude other filesystems"), NULL, FALSE },
	{ SEARCH_CONSTRAINT_TYPE_NONE, NULL, NULL, NULL, FALSE}
};

enum {
	SEARCH_CONSTRAINT_CONTAINS_THE_TEXT,
	SEARCH_CONSTRAINT_TYPE_SEPARATOR_00,
	SEARCH_CONSTRAINT_DATE_MODIFIED_BEFORE,
	SEARCH_CONSTRAINT_DATE_MODIFIED_AFTER,
	SEARCH_CONSTRAINT_TYPE_SEPARATOR_01,
	SEARCH_CONSTRAINT_SIZE_IS_MORE_THAN,
	SEARCH_CONSTRAINT_SIZE_IS_LESS_THAN,
	SEARCH_CONSTRAINT_FILE_IS_EMPTY,
	SEARCH_CONSTRAINT_TYPE_SEPARATOR_02,
	SEARCH_CONSTRAINT_OWNED_BY_USER,
	SEARCH_CONSTRAINT_OWNED_BY_GROUP,
	SEARCH_CONSTRAINT_OWNER_IS_UNRECOGNIZED,
	SEARCH_CONSTRAINT_TYPE_SEPARATOR_03,
	SEARCH_CONSTRAINT_FILE_IS_NOT_NAMED,
	SEARCH_CONSTRAINT_FILE_MATCHES_REGULAR_EXPRESSION,
	SEARCH_CONSTRAINT_TYPE_SEPARATOR_04,
	SEARCH_CONSTRAINT_SHOW_HIDDEN_FILES_AND_FOLDERS,
	SEARCH_CONSTRAINT_FOLLOW_SYMBOLIC_LINKS,
	SEARCH_CONSTRAINT_SEARCH_OTHER_FILESYSTEMS,
	SEARCH_CONSTRAINT_MAXIMUM_POSSIBLE
};

static CtkTargetEntry GSearchDndTable[] = {
	{ "text/uri-list", 0, 1 },
	{ "text/plain",    0, 0 },
	{ "STRING",        0, 0 }
};

static guint GSearchTotalDnds = sizeof (GSearchDndTable) / sizeof (GSearchDndTable[0]);

struct _GSearchGOptionArguments {
	gchar * name;
	gchar * path;
	gchar * contains;
	gchar * user;
	gchar * group;
	gboolean nouser;
	gchar * mtimeless;
	gchar * mtimemore;
	gchar * sizeless;
	gchar * sizemore;
	gboolean empty;
	gchar * notnamed;
	gchar * regex;
	gboolean hidden;
	gboolean follow;
	gboolean mounts;
	gchar * sortby;
	gboolean descending;
	gboolean start;
} GSearchGOptionArguments;

static GOptionEntry GSearchGOptionEntries[] = {
	{ "version", 0, G_OPTION_FLAG_NO_ARG, G_OPTION_ARG_CALLBACK, version_cb, N_("Show version of the application"), NULL},
	{ "named", 0, 0, G_OPTION_ARG_STRING, &GSearchGOptionArguments.name, NULL, N_("STRING") },
	{ "path", 0, 0, G_OPTION_ARG_STRING, &GSearchGOptionArguments.path, NULL, N_("PATH") },
	{ "sortby", 0, 0, G_OPTION_ARG_STRING, &GSearchGOptionArguments.sortby, NULL, N_("VALUE") },
	{ "descending", 0, 0, G_OPTION_ARG_NONE, &GSearchGOptionArguments.descending, NULL, NULL },
	{ "start", 0, 0, G_OPTION_ARG_NONE, &GSearchGOptionArguments.start, NULL, NULL },
	{ "contains", 0, 0, G_OPTION_ARG_STRING, &GSearchGOptionArguments.contains, NULL, N_("STRING") },
	{ "mtimeless", 0, 0, G_OPTION_ARG_STRING, &GSearchGOptionArguments.mtimeless, NULL, N_("DAYS") },
	{ "mtimemore", 0, 0, G_OPTION_ARG_STRING, &GSearchGOptionArguments.mtimemore, NULL, N_("DAYS") },
	{ "sizemore", 0, 0, G_OPTION_ARG_STRING, &GSearchGOptionArguments.sizemore, NULL, N_("KILOBYTES") },
	{ "sizeless", 0, 0, G_OPTION_ARG_STRING, &GSearchGOptionArguments.sizeless, NULL, N_("KILOBYTES") },
	{ "empty", 0, 0, G_OPTION_ARG_NONE, &GSearchGOptionArguments.empty, NULL, NULL },
	{ "user", 0, 0, G_OPTION_ARG_STRING, &GSearchGOptionArguments.user, NULL, N_("USER") },
	{ "group", 0, 0, G_OPTION_ARG_STRING, &GSearchGOptionArguments.group, NULL, N_("GROUP") },
	{ "nouser", 0, 0, G_OPTION_ARG_NONE, &GSearchGOptionArguments.nouser, NULL, NULL },
	{ "notnamed", 0, 0, G_OPTION_ARG_STRING, &GSearchGOptionArguments.notnamed, NULL, N_("STRING") },
	{ "regex", 0, 0, G_OPTION_ARG_STRING, &GSearchGOptionArguments.regex, NULL, N_("PATTERN") },
	{ "hidden", 0, 0, G_OPTION_ARG_NONE, &GSearchGOptionArguments.hidden, NULL, NULL },
	{ "follow", 0, 0, G_OPTION_ARG_NONE, &GSearchGOptionArguments.follow, NULL, NULL },
	{ "mounts", 0, 0, G_OPTION_ARG_NONE, &GSearchGOptionArguments.mounts, NULL, NULL },
	{ NULL }
};

static gchar * find_command_default_name_argument;
static gchar * locate_command_default_options;
pid_t locate_database_check_command_pid;

static gboolean
handle_locate_command_stdout_io (GIOChannel * ioc,
                                 GIOCondition condition,
                                 gpointer data)
{
	GSearchWindow * gsearch = data;
	gboolean broken_pipe = FALSE;

	if (condition & G_IO_IN) {

		GError * error = NULL;
		GString * string;

		string = g_string_new (NULL);

		while (ioc->is_readable != TRUE);

		do {
			gint status;

			do {
				status = g_io_channel_read_line_string (ioc, string, NULL, &error);

				if (status == G_IO_STATUS_EOF) {
					broken_pipe = TRUE;
				}
				else if (status == G_IO_STATUS_AGAIN) {
					if (ctk_events_pending ()) {
						while (ctk_events_pending ()) {
							ctk_main_iteration ();
						}
					}
				}
				else if ((string->len != 0) && (strncmp (string->str, "/", 1) == 0)) {
					gsearch->is_locate_database_available = TRUE;
					broken_pipe = TRUE;
				}

			} while (status == G_IO_STATUS_AGAIN && broken_pipe == FALSE);

			if (broken_pipe == TRUE) {
				break;
			}

			if (status != G_IO_STATUS_NORMAL) {
				if (error != NULL) {
					g_warning ("handle_locate_command_stdout_io(): %s", error->message);
					g_error_free (error);
				}
			}

		} while (g_io_channel_get_buffer_condition (ioc) & G_IO_IN);

		waitpid (locate_database_check_command_pid, NULL, 0);
		g_string_free (string, TRUE);
	}

	if (!(condition & G_IO_IN) || broken_pipe == TRUE) {
		gsearch->is_locate_database_check_finished = TRUE;
		g_io_channel_shutdown (ioc, TRUE, NULL);
		return FALSE;
	}
	return TRUE;
}

static void
setup_case_insensitive_arguments (GSearchWindow * gsearch)
{
	static gboolean case_insensitive_arguments_initialized = FALSE;
	gchar * cmd_stderr;
	gchar * grep_cmd;
	gchar * locate;

	if (case_insensitive_arguments_initialized == TRUE) {
		return;
	}
	case_insensitive_arguments_initialized = TRUE;

	/* check find command for -iname argument compatibility */
	g_spawn_command_line_sync ("find /dev/null -iname 'string'", NULL, &cmd_stderr, NULL, NULL);

	if ((cmd_stderr != NULL) && (strlen (cmd_stderr) == 0)) {
		find_command_default_name_argument = g_strdup ("-iname");
		GSearchOptionTemplates[SEARCH_CONSTRAINT_FILE_IS_NOT_NAMED].option = g_strdup ("'!' -iname '*%s*'");
	}
	else {
		find_command_default_name_argument = g_strdup ("-name");
	}
	g_free (cmd_stderr);

	/* check grep command for -i argument compatibility */
	grep_cmd = g_strdup_printf ("%s -i 'string' /dev/null", GREP_COMMAND);
 	g_spawn_command_line_sync (grep_cmd, NULL, &cmd_stderr, NULL, NULL);

	if ((cmd_stderr != NULL) && (strlen (cmd_stderr) == 0)) {
		g_free (cmd_stderr);
		g_free (grep_cmd);

		/* check grep command for -I argument compatibility, bug 568840 */
		grep_cmd = g_strdup_printf ("%s -i -I 'string' /dev/null", GREP_COMMAND);
	 	g_spawn_command_line_sync (grep_cmd, NULL, &cmd_stderr, NULL, NULL);

		if ((cmd_stderr != NULL) && (strlen (cmd_stderr) == 0)) {
	 		GSearchOptionTemplates[SEARCH_CONSTRAINT_CONTAINS_THE_TEXT].option =
			    g_strdup_printf ("'!' -type p -exec %s -i -I -c '%%s' {} \\;", GREP_COMMAND);
 		}
		else {
	 		GSearchOptionTemplates[SEARCH_CONSTRAINT_CONTAINS_THE_TEXT].option =
			    g_strdup_printf ("'!' -type p -exec %s -i -c '%%s' {} \\;", GREP_COMMAND);
		}
	}
	else {
 		GSearchOptionTemplates[SEARCH_CONSTRAINT_CONTAINS_THE_TEXT].option =
		    g_strdup_printf ("'!' -type p -exec %s -c '%%s' {} \\;", GREP_COMMAND);
	}
	g_free (cmd_stderr);

	locate = g_find_program_in_path ("locate");

	if (locate != NULL) {
		GIOChannel * ioc_stdout;
		gchar ** argv  = NULL;
		gchar *command = NULL;
		gint child_stdout;

		/* check locate command for -i argument compatibility */
		command = g_strconcat (locate, " -i /", NULL);
		g_shell_parse_argv (command, NULL, &argv, NULL);
		g_free (command);

		gsearch->is_locate_database_check_finished = FALSE;
		gsearch->is_locate_database_available = FALSE;

		/* run locate command asynchronously because on some systems it can be slow */
		if (g_spawn_async_with_pipes (g_get_home_dir (), argv, NULL,
				       G_SPAWN_SEARCH_PATH, NULL, NULL,
				       &locate_database_check_command_pid, NULL, &child_stdout,
				       NULL, NULL)) {

			ioc_stdout = g_io_channel_unix_new (child_stdout);
			g_io_channel_set_encoding (ioc_stdout, NULL, NULL);
			g_io_channel_set_flags (ioc_stdout, G_IO_FLAG_NONBLOCK, NULL);
			g_io_add_watch (ioc_stdout, G_IO_IN | G_IO_HUP,
					handle_locate_command_stdout_io, gsearch);
			g_io_channel_unref (ioc_stdout);
		}
		else {
			gsearch->is_locate_database_check_finished = TRUE;
		}

		g_strfreev (argv);

		while (gsearch->is_locate_database_check_finished == FALSE) {
			if (ctk_events_pending ()) {
				while (ctk_events_pending ()) {
					ctk_main_iteration ();
				}
			}
		}

		if (gsearch->is_locate_database_available == TRUE) {
			locate_command_default_options = g_strdup ("-i");
		}
		else {
			/* run locate again to check if it can find anything */
			command = g_strconcat (locate, " /", NULL);
			g_shell_parse_argv (command, NULL, &argv, NULL);
			g_free (command);

			gsearch->is_locate_database_check_finished = FALSE;
			locate_command_default_options = g_strdup ("");

			/* run locate command asynchronously because on some systems it can be slow */
			if (g_spawn_async_with_pipes (g_get_home_dir (), argv, NULL,
					       G_SPAWN_SEARCH_PATH, NULL, NULL,
					       &locate_database_check_command_pid, NULL, &child_stdout,
					       NULL, NULL)) {

				ioc_stdout = g_io_channel_unix_new (child_stdout);
				g_io_channel_set_encoding (ioc_stdout, NULL, NULL);
				g_io_channel_set_flags (ioc_stdout, G_IO_FLAG_NONBLOCK, NULL);
				g_io_add_watch (ioc_stdout, G_IO_IN | G_IO_HUP,
						handle_locate_command_stdout_io, gsearch);
				g_io_channel_unref (ioc_stdout);
			}
			else {
				gsearch->is_locate_database_check_finished = TRUE;
			}

			g_strfreev (argv);

			while (gsearch->is_locate_database_check_finished == FALSE) {
				if (ctk_events_pending ()) {
					while (ctk_events_pending ()) {
						ctk_main_iteration ();
					}
				}
			}

			if (gsearch->is_locate_database_available == FALSE) {
				g_warning (_("A locate database has probably not been created."));
			}
		}
	}
	else {
		/* locate is not installed */
		locate_command_default_options = g_strdup ("");
		gsearch->is_locate_database_available = FALSE;
	}
	g_free (grep_cmd);
	g_free (locate);
}

static gchar *
setup_find_name_options (gchar * file)
{
	/* This function builds the name options for the find command.  This in
	   done to insure that the find command returns hidden files and folders. */

	GString * command;
	command = g_string_new ("");

	if (strstr (file, "*") == NULL) {

		if ((strlen (file) == 0) || (file[0] != '.')) {
	 		g_string_append_printf (command, "\\( %s \"*%s*\" -o %s \".*%s*\" \\) ",
					find_command_default_name_argument, file,
					find_command_default_name_argument, file);
		}
		else {
			g_string_append_printf (command, "\\( %s \"*%s*\" -o %s \".*%s*\" -o %s \"%s*\" \\) ",
					find_command_default_name_argument, file,
					find_command_default_name_argument, file,
					find_command_default_name_argument, file);
		}
	}
	else {
		if (file[0] == '.') {
			g_string_append_printf (command, "\\( %s \"%s\" -o %s \".*%s\" \\) ",
					find_command_default_name_argument, file,
					find_command_default_name_argument, file);
		}
		else if (file[0] != '*') {
			g_string_append_printf (command, "%s \"%s\" ",
					find_command_default_name_argument, file);
		}
		else {
			if ((strlen (file) >= 1) && (file[1] == '.')) {
				g_string_append_printf (command, "\\( %s \"%s\" -o %s \"%s\" \\) ",
					find_command_default_name_argument, file,
					find_command_default_name_argument, &file[1]);
			}
			else {
				g_string_append_printf (command, "\\( %s \"%s\" -o %s \".%s\" \\) ",
					find_command_default_name_argument, file,
					find_command_default_name_argument, file);
			}
		}
	}
	return g_string_free (command, FALSE);
}

static gboolean
has_additional_constraints (GSearchWindow * gsearch)
{
	GList * list;

	if (gsearch->available_options_selected_list != NULL) {

		for (list = gsearch->available_options_selected_list; list != NULL; list = g_list_next (list)) {

			GSearchConstraint * constraint = list->data;

			switch (GSearchOptionTemplates[constraint->constraint_id].type) {
			case SEARCH_CONSTRAINT_TYPE_BOOLEAN:
			case SEARCH_CONSTRAINT_TYPE_NUMERIC:
			case SEARCH_CONSTRAINT_TYPE_DATE_BEFORE:
			case SEARCH_CONSTRAINT_TYPE_DATE_AFTER:
				return TRUE;
			case SEARCH_CONSTRAINT_TYPE_TEXT:
				if (strlen (constraint->data.text) > 0) {
					return TRUE;
				}
			default:
				break;
			}
		}
	}
	return FALSE;
}

static void
display_dialog_character_set_conversion_error (CtkWidget * window,
                                               gchar * string,
                                               GError * error)
{
	CtkWidget * dialog;

	dialog = ctk_message_dialog_new (CTK_WINDOW (window),
 	                                 CTK_DIALOG_DESTROY_WITH_PARENT,
	                                 CTK_MESSAGE_ERROR,
	                                 CTK_BUTTONS_OK,
	                                 _("Character set conversion failed for \"%s\""),
					 string);

	ctk_message_dialog_format_secondary_text (CTK_MESSAGE_DIALOG (dialog),
	                                          (error == NULL) ? " " : error->message, NULL);

	ctk_window_set_title (CTK_WINDOW (dialog), "");
	ctk_container_set_border_width (CTK_CONTAINER (dialog), 5);
	ctk_box_set_spacing (CTK_BOX (ctk_dialog_get_content_area (CTK_DIALOG (dialog))), 14);

	g_signal_connect (G_OBJECT (dialog),
	                  "response",
	                   G_CALLBACK (ctk_widget_destroy), NULL);

	ctk_widget_show (dialog);
}

static void
start_animation (GSearchWindow * gsearch, gboolean first_pass)
{
	if (first_pass == TRUE) {

		gchar *title = NULL;

		title = g_strconcat (_("Searching..."), " - ", _("Search for Files"), NULL);
		ctk_window_set_title (CTK_WINDOW (gsearch->window), title);

		ctk_label_set_text (CTK_LABEL (gsearch->files_found_label), "");
		if (g_settings_get_boolean (gsearch->cafe_desktop_interface_settings, "enable-animations")) {
			ctk_spinner_start (CTK_SPINNER (gsearch->progress_spinner));
			ctk_widget_show (gsearch->progress_spinner);
		}
		g_free (title);

		gsearch->focus = ctk_window_get_focus (CTK_WINDOW (gsearch->window));

		ctk_window_set_default (CTK_WINDOW (gsearch->window), gsearch->stop_button);
		ctk_widget_show (gsearch->stop_button);
		ctk_widget_set_sensitive (gsearch->stop_button, TRUE);
		ctk_widget_hide (gsearch->find_button);
		ctk_widget_set_sensitive (gsearch->find_button, FALSE);
		ctk_widget_set_sensitive (gsearch->search_results_vbox, TRUE);
		ctk_widget_set_sensitive (CTK_WIDGET (gsearch->search_results_tree_view), TRUE);
		ctk_widget_set_sensitive (gsearch->available_options_vbox, FALSE);
		ctk_widget_set_sensitive (gsearch->show_more_options_expander, FALSE);
		ctk_widget_set_sensitive (gsearch->name_and_folder_table, FALSE);
	}
}

static void
stop_animation (GSearchWindow * gsearch)
{
	ctk_spinner_stop (CTK_SPINNER (gsearch->progress_spinner));

	ctk_window_set_default (CTK_WINDOW (gsearch->window), gsearch->find_button);
	ctk_widget_set_sensitive (gsearch->available_options_vbox, TRUE);
	ctk_widget_set_sensitive (gsearch->show_more_options_expander, TRUE);
	ctk_widget_set_sensitive (gsearch->name_and_folder_table, TRUE);
	ctk_widget_set_sensitive (gsearch->find_button, TRUE);
	ctk_widget_hide (gsearch->progress_spinner);
	ctk_widget_hide (gsearch->stop_button);
	ctk_widget_show (gsearch->find_button);

	if (CTK_IS_MENU_ITEM (gsearch->search_results_save_results_as_item) == TRUE) {
		ctk_widget_set_sensitive (gsearch->search_results_save_results_as_item, TRUE);
	}

	if (ctk_window_get_focus (CTK_WINDOW (gsearch->window)) == NULL) {
		ctk_window_set_focus (CTK_WINDOW (gsearch->window), gsearch->focus);
	}
}

gchar *
build_search_command (GSearchWindow * gsearch,
                      gboolean first_pass)
{
	GString * command;
	GError * error = NULL;
	gchar * file_is_named_utf8;
	gchar * file_is_named_locale;
	gchar * file_is_named_escaped;
	gchar * file_is_named_backslashed;
	gchar * look_in_folder_locale;
	gchar * look_in_folder_escaped;
	gchar * look_in_folder_backslashed;

	start_animation (gsearch, first_pass);
	setup_case_insensitive_arguments (gsearch);

	file_is_named_utf8 = g_strdup ((gchar *) ctk_entry_get_text (CTK_ENTRY (gsearch_history_entry_get_entry
	                                         (GSEARCH_HISTORY_ENTRY (gsearch->name_contains_entry)))));

	if (!file_is_named_utf8 || !*file_is_named_utf8) {
		g_free (file_is_named_utf8);
		file_is_named_utf8 = g_strdup ("*");
	}
	else {
		gchar * locale;

		locale = g_locale_from_utf8 (file_is_named_utf8, -1, NULL, NULL, &error);
		if (locale == NULL) {
			stop_animation (gsearch);
			display_dialog_character_set_conversion_error (gsearch->window, file_is_named_utf8, error);
			g_free (file_is_named_utf8);
			g_error_free (error);
			return NULL;
		}
		gsearch_history_entry_prepend_text (GSEARCH_HISTORY_ENTRY (gsearch->name_contains_entry), file_is_named_utf8);

		if ((strstr (locale, "*") == NULL) && (strstr (locale, "?") == NULL)) {
			gchar *tmp;

			tmp = file_is_named_utf8;
			file_is_named_utf8 = g_strconcat ("*", file_is_named_utf8, "*", NULL);
			g_free (tmp);
		}

		g_free (locale);
	}

	file_is_named_locale = g_locale_from_utf8 (file_is_named_utf8, -1, NULL, NULL, &error);
	if (file_is_named_locale == NULL) {
		stop_animation (gsearch);
		display_dialog_character_set_conversion_error (gsearch->window, file_is_named_utf8, error);
		g_free (file_is_named_utf8);
		g_error_free (error);
		return NULL;
	}

	look_in_folder_locale = ctk_file_chooser_get_filename (CTK_FILE_CHOOSER (gsearch->look_in_folder_button));

	if (look_in_folder_locale == NULL) {
		/* If for some reason a path was not returned fallback to the user's home directory. */
		look_in_folder_locale = g_strdup (g_get_home_dir ());
		ctk_file_chooser_set_current_folder (CTK_FILE_CHOOSER (gsearch->look_in_folder_button), look_in_folder_locale);
	}

	if (!g_str_has_suffix (look_in_folder_locale, G_DIR_SEPARATOR_S)) {
		gchar *tmp;

		tmp = look_in_folder_locale;
		look_in_folder_locale = g_strconcat (look_in_folder_locale, G_DIR_SEPARATOR_S, NULL);
		g_free (tmp);
	}
	g_free (gsearch->command_details->look_in_folder_string);

	look_in_folder_backslashed = backslash_backslash_characters (look_in_folder_locale);
	look_in_folder_escaped = escape_double_quotes (look_in_folder_backslashed);
	gsearch->command_details->look_in_folder_string = g_strdup (look_in_folder_locale);

	command = g_string_new ("");
	gsearch->command_details->is_command_show_hidden_files_enabled = FALSE;
	gsearch->command_details->name_contains_regex_string = NULL;
	gsearch->command_details->name_contains_pattern_string = NULL;

	gsearch->command_details->is_command_first_pass = first_pass;
	if (gsearch->command_details->is_command_first_pass == TRUE) {
		gsearch->command_details->is_command_using_quick_mode = FALSE;
	}

	if ((ctk_widget_get_visible (gsearch->available_options_vbox) == FALSE) ||
	    (has_additional_constraints (gsearch) == FALSE)) {

		file_is_named_backslashed = backslash_backslash_characters (file_is_named_locale);
		file_is_named_escaped = escape_double_quotes (file_is_named_backslashed);
		gsearch->command_details->name_contains_pattern_string = g_strdup (file_is_named_utf8);

		if (gsearch->command_details->is_command_first_pass == TRUE) {

			gchar * locate;
			BaulSpeedTradeoff show_thumbnails_enum;
			gboolean disable_quick_search;

			locate = g_find_program_in_path ("locate");
			disable_quick_search = g_settings_get_boolean (gsearch->cafe_search_tool_settings, "disable-quick-search");
			gsearch->command_details->is_command_second_pass_enabled = !g_settings_get_boolean (gsearch->cafe_search_tool_settings, "disable-quick-search-second-scan");

			/* Use baul settings for thumbnails if baul is installed, else fall back to the baul default */
			if (gsearch->baul_schema_exists) {
				show_thumbnails_enum = g_settings_get_enum (gsearch->baul_settings, "show-image-thumbnails");
			} else {
				show_thumbnails_enum = SPEED_TRADEOFF_LOCAL_ONLY;
			}

			if (show_thumbnails_enum == SPEED_TRADEOFF_ALWAYS ||
				show_thumbnails_enum == SPEED_TRADEOFF_LOCAL_ONLY) {
				GVariant * value;
				guint64 size_limit = 10485760;

				if (gsearch->baul_schema_exists) {
					value = g_settings_get_value (gsearch->baul_settings, "thumbnail-limit");
					if (value) {
						size_limit = g_variant_get_uint64 (value);
						g_variant_unref (value);
					}
				}

				gsearch->show_thumbnails = TRUE;
				gsearch->show_thumbnails_file_size_limit = size_limit;
			}
			else {
				gsearch->show_thumbnails = FALSE;
				gsearch->show_thumbnails_file_size_limit = 0;
			}

			if ((disable_quick_search == FALSE)
			    && (gsearch->is_locate_database_available == TRUE)
			    && (locate != NULL)
			    && (is_quick_search_excluded_path (look_in_folder_locale) == FALSE)) {

					g_string_append_printf (command, "%s %s \"%s*%s\"",
							locate,
							locate_command_default_options,
							look_in_folder_escaped,
							file_is_named_escaped);
					gsearch->command_details->is_command_using_quick_mode = TRUE;
			}
			else {
				g_string_append_printf (command, "find \"%s\" %s \"%s\" -print",
							look_in_folder_escaped,
							find_command_default_name_argument,
							file_is_named_escaped);
			}
			g_free (locate);
		}
		else {
			g_string_append_printf (command, "find \"%s\" %s \"%s\" -print",
						look_in_folder_escaped,
						find_command_default_name_argument,
						file_is_named_escaped);
		}
	}
	else {
		GList * list;
		gboolean disable_mount_argument = TRUE;

		gsearch->command_details->is_command_regex_matching_enabled = FALSE;
		file_is_named_backslashed = backslash_backslash_characters (file_is_named_locale);
		file_is_named_escaped = escape_double_quotes (file_is_named_backslashed);

		g_string_append_printf (command, "find \"%s\" %s",
					look_in_folder_escaped,
					setup_find_name_options (file_is_named_escaped));

		for (list = gsearch->available_options_selected_list; list != NULL; list = g_list_next (list)) {

			GSearchConstraint * constraint = list->data;

			switch (GSearchOptionTemplates[constraint->constraint_id].type) {
			case SEARCH_CONSTRAINT_TYPE_BOOLEAN:
				if (strcmp (GSearchOptionTemplates[constraint->constraint_id].option, "EXCLUDE_OTHER_FILESYSTEMS") == 0) {
					disable_mount_argument = FALSE;
				}
				else if (strcmp (GSearchOptionTemplates[constraint->constraint_id].option, "SHOW_HIDDEN_FILES") == 0) {
					gsearch->command_details->is_command_show_hidden_files_enabled = TRUE;
				}
				else {
					g_string_append_printf (command, "%s ",
						GSearchOptionTemplates[constraint->constraint_id].option);
				}
				break;
			case SEARCH_CONSTRAINT_TYPE_TEXT:
				if (strcmp (GSearchOptionTemplates[constraint->constraint_id].option, "-regex '%s'") == 0) {

					gchar * escaped;
					gchar * regex;

					escaped = backslash_special_characters (constraint->data.text);
					regex = escape_single_quotes (escaped);

					if (regex != NULL) {
						gsearch->command_details->is_command_regex_matching_enabled = TRUE;
						gsearch->command_details->name_contains_regex_string = g_locale_from_utf8 (regex, -1, NULL, NULL, NULL);
					}

					g_free (escaped);
					g_free (regex);
				}
				else {
					gchar * escaped;
					gchar * backslashed;
					gchar * locale;

					backslashed = backslash_special_characters (constraint->data.text);
					escaped = escape_single_quotes (backslashed);

					locale = g_locale_from_utf8 (escaped, -1, NULL, NULL, NULL);

					if (strlen (locale) != 0) {
						g_string_append_printf (command,
							  	        GSearchOptionTemplates[constraint->constraint_id].option,
						  		        locale);

						g_string_append_c (command, ' ');
					}

					g_free (escaped);
					g_free (backslashed);
					g_free (locale);
				}
				break;
			case SEARCH_CONSTRAINT_TYPE_NUMERIC:
				g_string_append_printf (command,
					  		GSearchOptionTemplates[constraint->constraint_id].option,
							(constraint->data.number * 1024),
					  		(constraint->data.number * 1024));
				g_string_append_c (command, ' ');
				break;
			case SEARCH_CONSTRAINT_TYPE_DATE_BEFORE:
				g_string_append_printf (command,
					 		GSearchOptionTemplates[constraint->constraint_id].option,
					  		constraint->data.time);
				g_string_append_c (command, ' ');
				break;
			case SEARCH_CONSTRAINT_TYPE_DATE_AFTER:
				g_string_append_printf (command,
					 		GSearchOptionTemplates[constraint->constraint_id].option,
					  		constraint->data.time,
					  		constraint->data.time);
				g_string_append_c (command, ' ');
				break;
			default:
		        	break;
			}
		}
		gsearch->command_details->name_contains_pattern_string = g_strdup ("*");

		if (disable_mount_argument != TRUE) {
			g_string_append (command, "-xdev ");
		}

		g_string_append (command, "-print ");
	}
	g_free (file_is_named_locale);
	g_free (file_is_named_utf8);
	g_free (file_is_named_backslashed);
	g_free (file_is_named_escaped);
	g_free (look_in_folder_locale);
	g_free (look_in_folder_backslashed);
	g_free (look_in_folder_escaped);

	return g_string_free (command, FALSE);
}

static void
add_file_to_search_results (const gchar * file,
			    CtkListStore * store,
			    CtkTreeIter * iter,
			    GSearchWindow * gsearch)
{
	GdkPixbuf * pixbuf;
	GSearchMonitor * monitor;
	GFileMonitor * handle;
	GFileInfo * file_info;
	GFile * g_file;
	GError * error = NULL;
	GTimeVal time_val;
	CtkTreePath * path;
	CtkTreeRowReference * reference;
	gchar * description;
	gchar * readable_size;
	gchar * readable_date;
	gchar * utf8_base_name;
	gchar * utf8_relative_dir_name;
	gchar * base_name;
	gchar * dir_name;
	gchar * relative_dir_name;
	gchar * look_in_folder;

	if (g_hash_table_lookup_extended (gsearch->search_results_filename_hash_table, file, NULL, NULL) == TRUE) {
		return;
	}

	if ((g_file_test (file, G_FILE_TEST_EXISTS) != TRUE) &&
	    (g_file_test (file, G_FILE_TEST_IS_SYMLINK) != TRUE)) {
		return;
	}

	g_hash_table_insert (gsearch->search_results_filename_hash_table, g_strdup (file), NULL);

	if (ctk_tree_view_get_headers_visible (CTK_TREE_VIEW (gsearch->search_results_tree_view)) == FALSE) {
		ctk_tree_view_set_headers_visible (CTK_TREE_VIEW (gsearch->search_results_tree_view), TRUE);
	}

	g_file = g_file_new_for_path (file);
	file_info = g_file_query_info (g_file, "standard::*,time::modified,thumbnail::path", 0, NULL, NULL);

	pixbuf = get_file_pixbuf (gsearch, file_info);
	description = get_file_type_description (file, file_info);
		readable_size = g_format_size (g_file_info_get_size (file_info));

	g_file_info_get_modification_time (file_info, &time_val);
	readable_date = get_readable_date (gsearch->search_results_date_format, time_val.tv_sec);

	base_name = g_path_get_basename (file);
	dir_name = g_path_get_dirname (file);

	look_in_folder = g_strdup (gsearch->command_details->look_in_folder_string);
	if (strlen (look_in_folder) > 1) {
		gchar * path_str;

		if (g_str_has_suffix (look_in_folder, G_DIR_SEPARATOR_S) == TRUE) {
			look_in_folder[strlen (look_in_folder) - 1] = '\0';
		}
		path_str = g_path_get_dirname (look_in_folder);
		if (strcmp (path_str, G_DIR_SEPARATOR_S) == 0) {
			relative_dir_name = g_strconcat (&dir_name[strlen (path_str)], NULL);
		}
		else {
			relative_dir_name = g_strconcat (&dir_name[strlen (path_str) + 1], NULL);
		}
		g_free (path_str);
	}
	else {
		relative_dir_name = g_strdup (dir_name);
	}

	utf8_base_name = g_filename_display_basename (file);
	utf8_relative_dir_name = g_filename_display_name (relative_dir_name);

	ctk_list_store_append (CTK_LIST_STORE (store), iter);
	ctk_list_store_set (CTK_LIST_STORE (store), iter,
			    COLUMN_ICON, pixbuf,
			    COLUMN_NAME, utf8_base_name,
			    COLUMN_RELATIVE_PATH, utf8_relative_dir_name,
			    COLUMN_LOCALE_FILE, file,
			    COLUMN_READABLE_SIZE, readable_size,
			    COLUMN_SIZE, (-1) * (gdouble) g_file_info_get_size(file_info),
			    COLUMN_TYPE, (description != NULL) ? description : g_strdup (g_file_info_get_content_type (file_info)),
			    COLUMN_READABLE_DATE, readable_date,
			    COLUMN_DATE, (-1) * (gdouble) time_val.tv_sec,
			    COLUMN_NO_FILES_FOUND, FALSE,
			    -1);

	monitor = g_slice_new0 (GSearchMonitor);
	if (monitor) {
		path = ctk_tree_model_get_path (CTK_TREE_MODEL (store), iter);
		reference = ctk_tree_row_reference_new (CTK_TREE_MODEL (store), path);
		ctk_tree_path_free (path);

		handle = g_file_monitor_file (g_file, G_FILE_MONITOR_EVENT_DELETED, NULL, &error);

		if (error == NULL) {
			monitor->gsearch = gsearch;
			monitor->reference = reference;
			monitor->handle = handle;
			ctk_list_store_set (CTK_LIST_STORE (store), iter,
			                    COLUMN_MONITOR, monitor, -1);

			g_signal_connect (handle, "changed",
					  G_CALLBACK (file_changed_cb), monitor);
		}
		else {
			ctk_tree_row_reference_free (reference);
			g_slice_free (GSearchMonitor, monitor);
			g_clear_error (&error);
		}
	}

	g_object_unref (g_file);
	g_object_unref (file_info);
	g_free (base_name);
	g_free (dir_name);
	g_free (relative_dir_name);
	g_free (utf8_base_name);
	g_free (utf8_relative_dir_name);
	g_free (look_in_folder);
	g_free (description);
	g_free (readable_size);
	g_free (readable_date);
}

static void
add_no_files_found_message (GSearchWindow * gsearch)
{
	/* When the list is empty append a 'No Files Found.' message. */
	ctk_widget_set_sensitive (CTK_WIDGET (gsearch->search_results_tree_view), FALSE);
	ctk_tree_view_set_headers_visible (CTK_TREE_VIEW (gsearch->search_results_tree_view), FALSE);
	ctk_tree_view_column_set_visible (gsearch->search_results_folder_column, FALSE);
	ctk_tree_view_column_set_visible (gsearch->search_results_size_column, FALSE);
	ctk_tree_view_column_set_visible (gsearch->search_results_type_column, FALSE);
	ctk_tree_view_column_set_visible (gsearch->search_results_date_column, FALSE);
	ctk_tree_view_columns_autosize (CTK_TREE_VIEW (gsearch->search_results_tree_view));
	g_object_set (gsearch->search_results_name_cell_renderer,
	              "underline", PANGO_UNDERLINE_NONE,
	              "underline-set", FALSE,
	              NULL);
	ctk_list_store_append (CTK_LIST_STORE (gsearch->search_results_list_store), &gsearch->search_results_iter);
	ctk_list_store_set (CTK_LIST_STORE (gsearch->search_results_list_store), &gsearch->search_results_iter,
		    	    COLUMN_ICON, NULL,
			    COLUMN_NAME, _("No files found"),
			    COLUMN_RELATIVE_PATH, "",
			    COLUMN_LOCALE_FILE, "",
			    COLUMN_READABLE_SIZE, "",
			    COLUMN_SIZE, (gdouble) 0,
			    COLUMN_TYPE, "",
		    	    COLUMN_READABLE_DATE, "",
		    	    COLUMN_DATE, (gdouble) 0,
			    COLUMN_NO_FILES_FOUND, TRUE,
		    	    -1);
}

void
update_search_counts (GSearchWindow * gsearch)
{
	gchar * title_bar_string = NULL;
	gchar * message_string = NULL;
	gchar * stopped_string = NULL;
	gchar * tmp;
	gint total_files;

	if (gsearch->command_details->command_status == ABORTED) {
		stopped_string = g_strdup (_("(stopped)"));
	}

	total_files = ctk_tree_model_iter_n_children (CTK_TREE_MODEL (gsearch->search_results_list_store), NULL);

	if (total_files == 0) {
		title_bar_string = g_strdup (_("No Files Found"));
		message_string = g_strdup (_("No files found"));
		add_no_files_found_message (gsearch);
	}
	else {
		title_bar_string = g_strdup_printf (ngettext ("%'d File Found",
					                      "%'d Files Found",
					                      total_files),
						    total_files);
		message_string = g_strdup_printf (ngettext ("%'d file found",
					                    "%'d files found",
					                    total_files),
						  total_files);
	}

	if (stopped_string != NULL) {
		tmp = message_string;
		message_string = g_strconcat (message_string, " ", stopped_string, NULL);
		g_free (tmp);

		tmp = title_bar_string;
		title_bar_string = g_strconcat (title_bar_string, " ", stopped_string, NULL);
		g_free (tmp);
	}

	tmp = title_bar_string;
	title_bar_string = g_strconcat (title_bar_string, " - ", _("Search for Files"), NULL);
	ctk_window_set_title (CTK_WINDOW (gsearch->window), title_bar_string);
	g_free (tmp);

	ctk_label_set_text (CTK_LABEL (gsearch->files_found_label), message_string);

	g_free (title_bar_string);
	g_free (message_string);
	g_free (stopped_string);
}

static void
intermediate_file_count_update (GSearchWindow * gsearch)
{
	gchar * string;
	gint count;

	count = ctk_tree_model_iter_n_children (CTK_TREE_MODEL (gsearch->search_results_list_store), NULL);

	if (count > 0) {

		string = g_strdup_printf (ngettext ("%'d file found",
		                                    "%'d files found",
		                                    count),
		                          count);

		ctk_label_set_text (CTK_LABEL (gsearch->files_found_label), string);
		g_free (string);
	}
}

gboolean
tree_model_iter_free_monitor (CtkTreeModel * model,
                              CtkTreePath * path,
                              CtkTreeIter * iter,
                              gpointer data)
{
	GSearchMonitor * monitor;

	g_return_val_if_fail (CTK_IS_TREE_MODEL (model), FALSE);

	ctk_tree_model_get (model, iter, COLUMN_MONITOR, &monitor, -1);
	if (monitor) {
		g_file_monitor_cancel (monitor->handle);
		ctk_tree_row_reference_free (monitor->reference);
		g_slice_free (GSearchMonitor, monitor);
	}
	return FALSE;
}

static CtkTreeModel *
gsearch_create_list_of_templates (void)
{
	CtkListStore * store;
	CtkTreeIter iter;
	gint idx;

	store = ctk_list_store_new (1, G_TYPE_STRING);

	for (idx = 0; GSearchOptionTemplates[idx].type != SEARCH_CONSTRAINT_TYPE_NONE; idx++) {

		if (GSearchOptionTemplates[idx].type == SEARCH_CONSTRAINT_TYPE_SEPARATOR) {
		        ctk_list_store_append (store, &iter);
		        ctk_list_store_set (store, &iter, 0, "separator", -1);
		}
		else {
			gchar * text = remove_mnemonic_character (_(GSearchOptionTemplates[idx].desc));
			ctk_list_store_append (store, &iter);
		        ctk_list_store_set (store, &iter, 0, text, -1);
			g_free (text);
		}
	}
	return CTK_TREE_MODEL (store);
}

static void
set_constraint_info_defaults (GSearchConstraint * opt)
{
	switch (GSearchOptionTemplates[opt->constraint_id].type) {
	case SEARCH_CONSTRAINT_TYPE_BOOLEAN:
		break;
	case SEARCH_CONSTRAINT_TYPE_TEXT:
		opt->data.text = "";
		break;
	case SEARCH_CONSTRAINT_TYPE_NUMERIC:
		opt->data.number = 0;
		break;
	case SEARCH_CONSTRAINT_TYPE_DATE_BEFORE:
	case SEARCH_CONSTRAINT_TYPE_DATE_AFTER:
		opt->data.time = 0;
		break;
	default:
	        break;
	}
}

void
update_constraint_info (GSearchConstraint * constraint,
			gchar * info)
{
	switch (GSearchOptionTemplates[constraint->constraint_id].type) {
	case SEARCH_CONSTRAINT_TYPE_TEXT:
		constraint->data.text = info;
		break;
	case SEARCH_CONSTRAINT_TYPE_NUMERIC:
		sscanf (info, "%d", &constraint->data.number);
		break;
	case SEARCH_CONSTRAINT_TYPE_DATE_BEFORE:
	case SEARCH_CONSTRAINT_TYPE_DATE_AFTER:
		sscanf (info, "%d", &constraint->data.time);
		break;
	default:
		g_warning (_("Entry changed called for a non entry option!"));
		break;
	}
}

void
set_constraint_selected_state (GSearchWindow * gsearch,
                               gint constraint_id,
			       gboolean state)
{
	gint idx;

	GSearchOptionTemplates[constraint_id].is_selected = state;

	for (idx = 0; GSearchOptionTemplates[idx].type != SEARCH_CONSTRAINT_TYPE_NONE; idx++) {
		if (GSearchOptionTemplates[idx].is_selected == FALSE) {
			ctk_combo_box_set_active (CTK_COMBO_BOX (gsearch->available_options_combo_box), idx);
			ctk_widget_set_sensitive (gsearch->available_options_add_button, TRUE);
			ctk_widget_set_sensitive (gsearch->available_options_combo_box, TRUE);
			ctk_widget_set_sensitive (gsearch->available_options_label, TRUE);
			return;
		}
	}
	ctk_widget_set_sensitive (gsearch->available_options_add_button, FALSE);
	ctk_widget_set_sensitive (gsearch->available_options_combo_box, FALSE);
	ctk_widget_set_sensitive (gsearch->available_options_label, FALSE);
}

void
set_constraint_gsettings_boolean (gint constraint_id,
                                  gboolean flag)
{
	GSettings * select_settings;

	select_settings = g_settings_new ("org.cafe.search-tool.select");

	switch (constraint_id) {

		case SEARCH_CONSTRAINT_CONTAINS_THE_TEXT:
			g_settings_set_boolean (select_settings, "contains-the-text",
	   		       	       	       	flag);
			break;
		case SEARCH_CONSTRAINT_DATE_MODIFIED_BEFORE:
			g_settings_set_boolean (select_settings, "date-modified-less-than",
	   		       	       	       	flag);
			break;
		case SEARCH_CONSTRAINT_DATE_MODIFIED_AFTER:
			g_settings_set_boolean (select_settings, "date-modified-more-than",
	   		       	       	       	flag);
			break;
		case SEARCH_CONSTRAINT_SIZE_IS_MORE_THAN:
			g_settings_set_boolean (select_settings, "size-at-least",
	   		       	       		    flag);
			break;
		case SEARCH_CONSTRAINT_SIZE_IS_LESS_THAN:
			g_settings_set_boolean (select_settings, "size-at-most",
	   		       	       	        flag);
			break;
		case SEARCH_CONSTRAINT_FILE_IS_EMPTY:
			g_settings_set_boolean (select_settings, "file-is-empty",
	   		       	       	        flag);
			break;
		case SEARCH_CONSTRAINT_OWNED_BY_USER:
			g_settings_set_boolean (select_settings, "owned-by-user",
	   		       	       	        flag);
			break;
		case SEARCH_CONSTRAINT_OWNED_BY_GROUP:
			g_settings_set_boolean (select_settings, "owned-by-group",
	   		       	       	        flag);
			break;
		case SEARCH_CONSTRAINT_OWNER_IS_UNRECOGNIZED:
			g_settings_set_boolean (select_settings, "owner-is-unrecognized",
	   		       	       	        flag);
			break;
		case SEARCH_CONSTRAINT_FILE_IS_NOT_NAMED:
			g_settings_set_boolean (select_settings, "name-does-not-contain",
	   		       	       	        flag);
			break;
		case SEARCH_CONSTRAINT_FILE_MATCHES_REGULAR_EXPRESSION:
			g_settings_set_boolean (select_settings, "name-matches-regular-expression",
	   		       	       	        flag);
			break;
		case SEARCH_CONSTRAINT_SHOW_HIDDEN_FILES_AND_FOLDERS:
			g_settings_set_boolean (select_settings, "show-hidden-files-and-folders",
	   		       	       	        flag);
			break;
		case SEARCH_CONSTRAINT_FOLLOW_SYMBOLIC_LINKS:
			g_settings_set_boolean (select_settings, "follow-symbolic-links",
	   		       	       	        flag);
			break;
		case SEARCH_CONSTRAINT_SEARCH_OTHER_FILESYSTEMS:
			g_settings_set_boolean (select_settings, "exclude-other-filesystems",
	   		       	       	        flag);
			break;

		default:
			break;
	}
	g_object_unref (select_settings);
}

/*
 * add_atk_namedesc
 * @widget    : The Ctk Widget for which @name and @desc are added.
 * @name      : Accessible Name
 * @desc      : Accessible Description
 * Description: This function adds accessible name and description to a
 *              Ctk widget.
 */

static void
add_atk_namedesc (CtkWidget * widget,
		  const gchar * name,
		  const gchar * desc)
{
	AtkObject * atk_widget;

	g_assert (CTK_IS_WIDGET (widget));

	atk_widget = ctk_widget_get_accessible (widget);

	if (name != NULL)
		atk_object_set_name (atk_widget, name);
	if (desc !=NULL)
		atk_object_set_description (atk_widget, desc);
}

/*
 * add_atk_relation
 * @obj1      : The first widget in the relation @rel_type
 * @obj2      : The second widget in the relation @rel_type.
 * @rel_type  : Relation type which relates @obj1 and @obj2
 * Description: This function establishes Atk Relation between two given
 *              objects.
 */

static void
add_atk_relation (CtkWidget * obj1,
		  CtkWidget * obj2,
		  AtkRelationType rel_type)
{
	AtkObject * atk_obj1, * atk_obj2;
	AtkRelationSet * relation_set;
	AtkRelation * relation;

	g_assert (CTK_IS_WIDGET (obj1));
	g_assert (CTK_IS_WIDGET (obj2));

	atk_obj1 = ctk_widget_get_accessible (obj1);

	atk_obj2 = ctk_widget_get_accessible (obj2);

	relation_set = atk_object_ref_relation_set (atk_obj1);
	relation = atk_relation_new (&atk_obj2, 1, rel_type);
	atk_relation_set_add (relation_set, relation);
	g_object_unref (G_OBJECT (relation));

}

static void
gsearch_setup_goption_descriptions (void)
{
	gint i = 1;
	gint j;

	GSearchGOptionEntries[i++].description = g_strdup (_("Set the text of \"Name contains\" search option"));
	GSearchGOptionEntries[i++].description = g_strdup (_("Set the text of \"Look in folder\" search option"));
  	GSearchGOptionEntries[i++].description = g_strdup (_("Sort files by one of the following: name, folder, size, type, or date"));
  	GSearchGOptionEntries[i++].description = g_strdup (_("Set sort order to descending, the default is ascending"));
  	GSearchGOptionEntries[i++].description = g_strdup (_("Automatically start a search"));

	for (j = 0; GSearchOptionTemplates[j].type != SEARCH_CONSTRAINT_TYPE_NONE; j++) {
		if (GSearchOptionTemplates[j].type != SEARCH_CONSTRAINT_TYPE_SEPARATOR) {
			gchar *text = remove_mnemonic_character (_(GSearchOptionTemplates[j].desc));
			if (GSearchOptionTemplates[j].type == SEARCH_CONSTRAINT_TYPE_BOOLEAN) {
				GSearchGOptionEntries[i++].description = g_strdup_printf (_("Select the \"%s\" search option"), text);
			}
			else {
				GSearchGOptionEntries[i++].description = g_strdup_printf (_("Select and set the \"%s\" search option"), text);
			}
			g_free (text);
		}
	}
}

static gboolean
handle_goption_args (GSearchWindow * gsearch)
{
	gboolean goption_args_found = FALSE;
	gint sort_by;

	if (GSearchGOptionArguments.name != NULL) {
		goption_args_found = TRUE;
		ctk_entry_set_text (CTK_ENTRY (gsearch_history_entry_get_entry (GSEARCH_HISTORY_ENTRY (gsearch->name_contains_entry))),
				    g_locale_to_utf8 (GSearchGOptionArguments.name, -1, NULL, NULL, NULL));
	}
	if (GSearchGOptionArguments.path != NULL) {
		goption_args_found = TRUE;
		ctk_file_chooser_set_current_folder (CTK_FILE_CHOOSER (gsearch->look_in_folder_button),
		                               g_locale_to_utf8 (GSearchGOptionArguments.path, -1, NULL, NULL, NULL));
	}
	if (GSearchGOptionArguments.contains != NULL) {
		goption_args_found = TRUE;
		add_constraint (gsearch, SEARCH_CONSTRAINT_CONTAINS_THE_TEXT,
				GSearchGOptionArguments.contains, TRUE);
	}
	if (GSearchGOptionArguments.mtimeless != NULL) {
		goption_args_found = TRUE;
		add_constraint (gsearch, SEARCH_CONSTRAINT_DATE_MODIFIED_BEFORE,
				GSearchGOptionArguments.mtimeless, TRUE);
	}
	if (GSearchGOptionArguments.mtimemore != NULL) {
		goption_args_found = TRUE;
		add_constraint (gsearch, SEARCH_CONSTRAINT_DATE_MODIFIED_AFTER,
				GSearchGOptionArguments.mtimemore, TRUE);
	}
	if (GSearchGOptionArguments.sizemore != NULL) {
		goption_args_found = TRUE;
		add_constraint (gsearch, SEARCH_CONSTRAINT_SIZE_IS_MORE_THAN,
				GSearchGOptionArguments.sizemore, TRUE);
	}
	if (GSearchGOptionArguments.sizeless != NULL) {
		goption_args_found = TRUE;
		add_constraint (gsearch, SEARCH_CONSTRAINT_SIZE_IS_LESS_THAN,
				GSearchGOptionArguments.sizeless, TRUE);
	}
	if (GSearchGOptionArguments.empty) {
		goption_args_found = TRUE;
		add_constraint (gsearch, SEARCH_CONSTRAINT_FILE_IS_EMPTY, NULL, TRUE);
	}
	if (GSearchGOptionArguments.user != NULL) {
		goption_args_found = TRUE;
		add_constraint (gsearch, SEARCH_CONSTRAINT_OWNED_BY_USER,
				GSearchGOptionArguments.user, TRUE);
	}
	if (GSearchGOptionArguments.group != NULL) {
		goption_args_found = TRUE;
		add_constraint (gsearch, SEARCH_CONSTRAINT_OWNED_BY_GROUP,
				GSearchGOptionArguments.group, TRUE);
	}
	if (GSearchGOptionArguments.nouser) {
		goption_args_found = TRUE;
		add_constraint (gsearch, SEARCH_CONSTRAINT_OWNER_IS_UNRECOGNIZED, NULL, TRUE);
	}
	if (GSearchGOptionArguments.notnamed != NULL) {
		goption_args_found = TRUE;
		add_constraint (gsearch, SEARCH_CONSTRAINT_FILE_IS_NOT_NAMED,
				GSearchGOptionArguments.notnamed, TRUE);
	}
	if (GSearchGOptionArguments.regex != NULL) {
		goption_args_found = TRUE;
		add_constraint (gsearch, SEARCH_CONSTRAINT_FILE_MATCHES_REGULAR_EXPRESSION,
				GSearchGOptionArguments.regex, TRUE);
	}
	if (GSearchGOptionArguments.hidden) {
		goption_args_found = TRUE;
		add_constraint (gsearch, SEARCH_CONSTRAINT_SHOW_HIDDEN_FILES_AND_FOLDERS, NULL, TRUE);
	}
	if (GSearchGOptionArguments.follow) {
		goption_args_found = TRUE;
		add_constraint (gsearch, SEARCH_CONSTRAINT_FOLLOW_SYMBOLIC_LINKS, NULL, TRUE);
	}
	if (GSearchGOptionArguments.mounts) {
		goption_args_found = TRUE;
		add_constraint (gsearch, SEARCH_CONSTRAINT_SEARCH_OTHER_FILESYSTEMS, NULL, TRUE);
	}
	if (GSearchGOptionArguments.sortby != NULL) {

		goption_args_found = TRUE;
		if (strcmp (GSearchGOptionArguments.sortby, "name") == 0) {
			sort_by = COLUMN_NAME;
		}
		else if (strcmp (GSearchGOptionArguments.sortby, "folder") == 0) {
			sort_by = COLUMN_RELATIVE_PATH;
		}
		else if (strcmp (GSearchGOptionArguments.sortby, "size") == 0) {
			sort_by = COLUMN_SIZE;
		}
		else if (strcmp (GSearchGOptionArguments.sortby, "type") == 0) {
			sort_by = COLUMN_TYPE;
		}
		else if (strcmp (GSearchGOptionArguments.sortby, "date") == 0) {
			sort_by = COLUMN_DATE;
		}
		else {
			g_warning (_("Invalid option passed to sortby command line argument."));
			sort_by = COLUMN_NAME;
		}

		if (GSearchGOptionArguments.descending) {
			ctk_tree_sortable_set_sort_column_id (CTK_TREE_SORTABLE (gsearch->search_results_list_store), sort_by,
							      CTK_SORT_DESCENDING);
		}
		else {
			ctk_tree_sortable_set_sort_column_id (CTK_TREE_SORTABLE (gsearch->search_results_list_store), sort_by,
							      CTK_SORT_ASCENDING);
		}
	}
	if (GSearchGOptionArguments.start) {
		goption_args_found = TRUE;
		click_find_cb (gsearch->find_button, (gpointer) gsearch);
	}
	return goption_args_found;
}

static gboolean
handle_search_command_stdout_io (GIOChannel * ioc,
				 GIOCondition condition,
				 gpointer data)
{
	GSearchWindow * gsearch = data;
	gboolean broken_pipe = FALSE;

	if (condition & G_IO_IN) {

		GError * error = NULL;
		GTimer * timer;
		GString * string;
		CdkRectangle prior_rect;
		CdkRectangle after_rect;
		gulong duration;
		gint look_in_folder_string_length;

		string = g_string_new (NULL);
		look_in_folder_string_length = strlen (gsearch->command_details->look_in_folder_string);

		timer = g_timer_new ();
		g_timer_start (timer);

		while (ioc->is_readable != TRUE);

		do {
			gchar * utf8 = NULL;
			gchar * filename = NULL;
			gint status;

			if (gsearch->command_details->command_status == MAKE_IT_STOP) {
				broken_pipe = TRUE;
				break;
			}
			else if (gsearch->command_details->command_status != RUNNING) {
			 	break;
			}

			do {
				status = g_io_channel_read_line_string (ioc, string, NULL, &error);

				if (status == G_IO_STATUS_EOF) {
					broken_pipe = TRUE;
				}
				else if (status == G_IO_STATUS_AGAIN) {
					if (ctk_events_pending ()) {
						intermediate_file_count_update (gsearch);
						while (ctk_events_pending ()) {
							if (gsearch->command_details->command_status == MAKE_IT_QUIT) {
								return FALSE;
							}
							ctk_main_iteration ();
						}

					}
				}

			} while (status == G_IO_STATUS_AGAIN && broken_pipe == FALSE);

			if (broken_pipe == TRUE) {
				break;
			}

			if (status != G_IO_STATUS_NORMAL) {
				if (error != NULL) {
					g_warning ("handle_search_command_stdout_io(): %s", error->message);
					g_error_free (error);
				}
				continue;
			}

			string = g_string_truncate (string, string->len - 1);
			if (string->len <= 1) {
				continue;
			}

			utf8 = g_filename_display_name (string->str);
			if (utf8 == NULL) {
				continue;
			}

			if (strncmp (string->str, gsearch->command_details->look_in_folder_string, look_in_folder_string_length) == 0) {

				if (strlen (string->str) != look_in_folder_string_length) {

					filename = g_path_get_basename (utf8);

					if (fnmatch (gsearch->command_details->name_contains_pattern_string, filename, FNM_NOESCAPE | FNM_CASEFOLD ) != FNM_NOMATCH) {
						if (gsearch->command_details->is_command_show_hidden_files_enabled) {
							if (gsearch->command_details->is_command_regex_matching_enabled == FALSE) {
								add_file_to_search_results (string->str, gsearch->search_results_list_store, &gsearch->search_results_iter, gsearch);
							}
							else if (compare_regex (gsearch->command_details->name_contains_regex_string, filename)) {
								add_file_to_search_results (string->str, gsearch->search_results_list_store, &gsearch->search_results_iter, gsearch);
							}
						}
						else if ((is_path_hidden (string->str) == FALSE ||
						          is_path_hidden (gsearch->command_details->look_in_folder_string) == TRUE) &&
						          (!g_str_has_suffix (string->str, "~"))) {
							if (gsearch->command_details->is_command_regex_matching_enabled == FALSE) {
								add_file_to_search_results (string->str, gsearch->search_results_list_store, &gsearch->search_results_iter, gsearch);
							}
							else if (compare_regex (gsearch->command_details->name_contains_regex_string, filename)) {
								add_file_to_search_results (string->str, gsearch->search_results_list_store, &gsearch->search_results_iter, gsearch);
							}
						}
					}
				}
			}
			g_free (utf8);
			g_free (filename);

			ctk_tree_view_get_visible_rect (CTK_TREE_VIEW (gsearch->search_results_tree_view), &prior_rect);

			if (prior_rect.y == 0) {
				ctk_tree_view_get_visible_rect (CTK_TREE_VIEW (gsearch->search_results_tree_view), &after_rect);
				if (after_rect.y <= 40) {  /* limit this hack to the first few pixels */
					ctk_tree_view_scroll_to_point (CTK_TREE_VIEW (gsearch->search_results_tree_view), -1, 0);
				}
			}

			g_timer_elapsed (timer, &duration);

			if (duration > CAFE_SEARCH_TOOL_REFRESH_DURATION) {
				if (ctk_events_pending ()) {
					intermediate_file_count_update (gsearch);
					while (ctk_events_pending ()) {
						if (gsearch->command_details->command_status == MAKE_IT_QUIT) {
							return FALSE;
						}
						ctk_main_iteration ();
					}
				}
				g_timer_reset (timer);
			}

		} while (g_io_channel_get_buffer_condition (ioc) & G_IO_IN);

		g_string_free (string, TRUE);
		g_timer_destroy (timer);
	}

	if (!(condition & G_IO_IN) || broken_pipe == TRUE) {

		g_io_channel_shutdown (ioc, TRUE, NULL);

		if ((gsearch->command_details->command_status != MAKE_IT_STOP)
		     && (gsearch->command_details->is_command_using_quick_mode == TRUE)
		     && (gsearch->command_details->is_command_first_pass == TRUE)
		     && (gsearch->command_details->is_command_second_pass_enabled == TRUE)
		     && (is_second_scan_excluded_path (gsearch->command_details->look_in_folder_string) == FALSE)) {

			gchar * command;

			/* Free these strings now because they are reassign values during the second pass. */
			g_free (gsearch->command_details->name_contains_pattern_string);
			g_free (gsearch->command_details->name_contains_regex_string);

			command = build_search_command (gsearch, FALSE);
			if (command != NULL) {
				spawn_search_command (gsearch, command);
				g_free (command);
			}
		}
		else {
			gsearch->command_details->command_status = (gsearch->command_details->command_status == MAKE_IT_STOP) ? ABORTED : STOPPED;
			gsearch->command_details->is_command_timeout_enabled = TRUE;
			g_hash_table_destroy (gsearch->search_results_pixbuf_hash_table);
			g_hash_table_destroy (gsearch->search_results_filename_hash_table);
			g_timeout_add (500, not_running_timeout_cb, (gpointer) gsearch);

			update_search_counts (gsearch);
			stop_animation (gsearch);

			/* Free the gchar fields of search_command structure. */
			g_free (gsearch->command_details->name_contains_pattern_string);
			g_free (gsearch->command_details->name_contains_regex_string);

		}
		return FALSE;
	}
	return TRUE;
}

static gboolean
handle_search_command_stderr_io (GIOChannel * ioc,
				 GIOCondition condition,
				 gpointer data)
{
	GSearchWindow * gsearch = data;
	static GString * error_msgs = NULL;
	static gboolean truncate_error_msgs = FALSE;
	gboolean broken_pipe = FALSE;

	if (condition & G_IO_IN) {

		GString * string;
		GError * error = NULL;
		gchar * utf8 = NULL;

		string = g_string_new (NULL);

		if (error_msgs == NULL) {
			error_msgs = g_string_new (NULL);
		}

		while (ioc->is_readable != TRUE);

		do {
			gint status;

			do {
				status = g_io_channel_read_line_string (ioc, string, NULL, &error);

				if (status == G_IO_STATUS_EOF) {
					broken_pipe = TRUE;
				}
				else if (status == G_IO_STATUS_AGAIN) {
					if (ctk_events_pending ()) {
						intermediate_file_count_update (gsearch);
						while (ctk_events_pending ()) {
							if (gsearch->command_details->command_status == MAKE_IT_QUIT) {
								break;
							}
							ctk_main_iteration ();

						}
					}
				}

			} while (status == G_IO_STATUS_AGAIN && broken_pipe == FALSE);

			if (broken_pipe == TRUE) {
				break;
			}

			if (status != G_IO_STATUS_NORMAL) {
				if (error != NULL) {
					g_warning ("handle_search_command_stderr_io(): %s", error->message);
					g_error_free (error);
				}
				continue;
			}

			if (truncate_error_msgs == FALSE) {
				if ((strstr (string->str, "ermission denied") == NULL) &&
			   	    (strstr (string->str, "No such file or directory") == NULL) &&
			   	    (strncmp (string->str, "grep: ", 6) != 0) &&
			 	    (strcmp (string->str, "find: ") != 0)) {
					utf8 = g_locale_to_utf8 (string->str, -1, NULL, NULL, NULL);
					error_msgs = g_string_append (error_msgs, utf8);
					truncate_error_msgs = limit_string_to_x_lines (error_msgs, 20);
				}
			}

		} while (g_io_channel_get_buffer_condition (ioc) & G_IO_IN);

		g_string_free (string, TRUE);
		g_free (utf8);
	}

	if (!(condition & G_IO_IN) || broken_pipe == TRUE) {

		if (error_msgs != NULL) {

			if (error_msgs->len > 0) {

				CtkWidget * dialog;

				if (truncate_error_msgs) {
					error_msgs = g_string_append (error_msgs,
				     		_("\n... Too many errors to display ..."));
				}

				if (gsearch->command_details->is_command_using_quick_mode != TRUE) {

					CtkWidget * hbox;
					CtkWidget * spacer;
					CtkWidget * expander;
					CtkWidget * label;

					dialog = ctk_message_dialog_new (CTK_WINDOW (gsearch->window),
					                                 CTK_DIALOG_DESTROY_WITH_PARENT,
					                                 CTK_MESSAGE_ERROR,
					                                 CTK_BUTTONS_OK,
					                                 _("The search results may be invalid."
									   "  There were errors while performing this search."));
					ctk_message_dialog_format_secondary_text (CTK_MESSAGE_DIALOG (dialog), " ");

					ctk_window_set_title (CTK_WINDOW (dialog), "");
					ctk_container_set_border_width (CTK_CONTAINER (dialog), 5);

					hbox = ctk_box_new (CTK_ORIENTATION_HORIZONTAL, 0);

					spacer = ctk_label_new ("     ");
					ctk_box_pack_start (CTK_BOX (hbox), spacer, FALSE, FALSE, 0);

					expander = ctk_expander_new_with_mnemonic (_("Show more _details"));
					ctk_container_set_border_width (CTK_CONTAINER (expander), 6);
					ctk_expander_set_spacing (CTK_EXPANDER (expander), 6);
					ctk_box_pack_start (CTK_BOX (hbox), expander, TRUE, TRUE, 0);

					label = ctk_label_new (error_msgs->str);
					ctk_container_add (CTK_CONTAINER (expander), label);

					ctk_box_pack_start (CTK_BOX (ctk_dialog_get_content_area (CTK_DIALOG (dialog))), hbox, FALSE, FALSE, 0);
					ctk_widget_show_all (hbox);

					g_signal_connect (G_OBJECT (dialog),
					                  "response",
					                   G_CALLBACK (ctk_widget_destroy), NULL);

					ctk_widget_show (dialog);
				}
				else if ((gsearch->command_details->is_command_second_pass_enabled == FALSE) ||
					 (is_second_scan_excluded_path (gsearch->command_details->look_in_folder_string) == TRUE)) {

					CtkWidget * button;
					CtkWidget * hbox;
					CtkWidget * spacer;
					CtkWidget * expander;
					CtkWidget * label;

					dialog = ctk_message_dialog_new (CTK_WINDOW (gsearch->window),
					                                 CTK_DIALOG_DESTROY_WITH_PARENT,
					                                 CTK_MESSAGE_ERROR,
					                                 CTK_BUTTONS_CANCEL,
					                                 _("The search results may be out of date or invalid."
									   "  Do you want to disable the quick search feature?"));
					ctk_message_dialog_format_secondary_text (CTK_MESSAGE_DIALOG (dialog),
					                                          "Please reference the help documentation for instructions "
										  "on how to configure and enable quick searches.");

					ctk_window_set_title (CTK_WINDOW (dialog), "");
					ctk_container_set_border_width (CTK_CONTAINER (dialog), 5);

					hbox = ctk_box_new (CTK_ORIENTATION_HORIZONTAL, 0);

					spacer = ctk_label_new ("     ");
					ctk_box_pack_start (CTK_BOX (hbox), spacer, FALSE, FALSE, 0);

					expander = ctk_expander_new_with_mnemonic (_("Show more _details"));
					ctk_container_set_border_width (CTK_CONTAINER (expander), 6);
					ctk_expander_set_spacing (CTK_EXPANDER (expander), 6);
					ctk_box_pack_start (CTK_BOX (hbox), expander, TRUE, TRUE, 0);

					label = ctk_label_new (error_msgs->str);
					ctk_container_add (CTK_CONTAINER (expander), label);

					ctk_box_pack_start (CTK_BOX (ctk_dialog_get_content_area (CTK_DIALOG (dialog))), hbox, FALSE, FALSE, 0);
					ctk_widget_show_all (hbox);

					button = gsearchtool_button_new_with_stock_icon (_("Disable _Quick Search"), CTK_STOCK_OK);
					ctk_widget_set_can_default (button, TRUE);
					ctk_widget_show (button);

					ctk_dialog_add_action_widget (CTK_DIALOG (dialog), button, CTK_RESPONSE_OK);
					ctk_dialog_set_default_response (CTK_DIALOG (dialog), CTK_RESPONSE_OK);

					g_signal_connect (G_OBJECT (dialog),
					                  "response",
					                  G_CALLBACK (disable_quick_search_cb), (gpointer) gsearch);

					ctk_widget_show (dialog);
				}
			}
			truncate_error_msgs = FALSE;
			g_string_truncate (error_msgs, 0);
		}
		g_io_channel_shutdown (ioc, TRUE, NULL);
		return FALSE;
	}
	return TRUE;
}

static void
child_command_set_pgid_cb (gpointer data)
{
	if (setpgid (0, 0) < 0) {
		g_print (_("Failed to set process group id of child %d: %s.\n"),
		         getpid (), g_strerror (errno));
	}
}

void
spawn_search_command (GSearchWindow * gsearch,
                      gchar * command)
{
	GIOChannel * ioc_stdout;
	GIOChannel * ioc_stderr;
	GError * error = NULL;
	gchar ** argv  = NULL;
	gint child_stdout;
	gint child_stderr;

	if (!g_shell_parse_argv (command, NULL, &argv, &error)) {
		CtkWidget * dialog;

		stop_animation (gsearch);

		dialog = ctk_message_dialog_new (CTK_WINDOW (gsearch->window),
		                                 CTK_DIALOG_DESTROY_WITH_PARENT,
		                                 CTK_MESSAGE_ERROR,
		                                 CTK_BUTTONS_OK,
		                                 _("Error parsing the search command."));
		ctk_message_dialog_format_secondary_text (CTK_MESSAGE_DIALOG (dialog),
		                                          (error == NULL) ? " " : error->message, NULL);

		ctk_window_set_title (CTK_WINDOW (dialog), "");
		ctk_container_set_border_width (CTK_CONTAINER (dialog), 5);
		ctk_box_set_spacing (CTK_BOX (ctk_dialog_get_content_area (CTK_DIALOG (dialog))), 14);

		ctk_dialog_run (CTK_DIALOG (dialog));
		ctk_widget_destroy (dialog);
		g_error_free (error);
		g_strfreev (argv);

		/* Free the gchar fields of search_command structure. */
		g_free (gsearch->command_details->look_in_folder_string);
		g_free (gsearch->command_details->name_contains_pattern_string);
		g_free (gsearch->command_details->name_contains_regex_string);
		return;
	}

	if (!g_spawn_async_with_pipes (g_get_home_dir (), argv, NULL,
				       G_SPAWN_SEARCH_PATH,
				       child_command_set_pgid_cb, NULL, &gsearch->command_details->command_pid, NULL, &child_stdout,
				       &child_stderr, &error)) {
		CtkWidget * dialog;

		stop_animation (gsearch);

		dialog = ctk_message_dialog_new (CTK_WINDOW (gsearch->window),
		                                 CTK_DIALOG_DESTROY_WITH_PARENT,
		                                 CTK_MESSAGE_ERROR,
		                                 CTK_BUTTONS_OK,
		                                 _("Error running the search command."));
		ctk_message_dialog_format_secondary_text (CTK_MESSAGE_DIALOG (dialog),
		                                          (error == NULL) ? " " : error->message, NULL);

		ctk_window_set_title (CTK_WINDOW (dialog), "");
		ctk_container_set_border_width (CTK_CONTAINER (dialog), 5);
		ctk_box_set_spacing (CTK_BOX (ctk_dialog_get_content_area (CTK_DIALOG (dialog))), 14);

		ctk_dialog_run (CTK_DIALOG (dialog));
		ctk_widget_destroy (dialog);
		g_error_free (error);
		g_strfreev (argv);

		/* Free the gchar fields of search_command structure. */
		g_free (gsearch->command_details->look_in_folder_string);
		g_free (gsearch->command_details->name_contains_pattern_string);
		g_free (gsearch->command_details->name_contains_regex_string);
		return;
	}

	if (gsearch->command_details->is_command_first_pass == TRUE) {

		gsearch->command_details->command_status = RUNNING;
		gsearch->search_results_pixbuf_hash_table = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_object_unref);
		gsearch->search_results_filename_hash_table = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

		/* Get the value of the baul date-format key if available. */
		if (gsearch->baul_schema_exists) {
			gsearch->search_results_date_format = g_settings_get_enum (gsearch->baul_settings, "date-format");
		} else {
			gsearch->search_results_date_format = BAUL_DATE_FORMAT_LOCALE;
		}

		ctk_tree_view_scroll_to_point (CTK_TREE_VIEW (gsearch->search_results_tree_view), 0, 0);
		ctk_tree_model_foreach (CTK_TREE_MODEL (gsearch->search_results_list_store),
					(CtkTreeModelForeachFunc) tree_model_iter_free_monitor, gsearch);
		ctk_list_store_clear (CTK_LIST_STORE (gsearch->search_results_list_store));

		ctk_tree_view_column_set_visible (gsearch->search_results_folder_column, TRUE);
		ctk_tree_view_column_set_visible (gsearch->search_results_size_column, TRUE);
		ctk_tree_view_column_set_visible (gsearch->search_results_type_column, TRUE);
		ctk_tree_view_column_set_visible (gsearch->search_results_date_column, TRUE);
	}

	ioc_stdout = g_io_channel_unix_new (child_stdout);
	ioc_stderr = g_io_channel_unix_new (child_stderr);

	g_io_channel_set_encoding (ioc_stdout, NULL, NULL);
	g_io_channel_set_encoding (ioc_stderr, NULL, NULL);

	g_io_channel_set_flags (ioc_stdout, G_IO_FLAG_NONBLOCK, NULL);
	g_io_channel_set_flags (ioc_stderr, G_IO_FLAG_NONBLOCK, NULL);

	g_io_add_watch (ioc_stdout, G_IO_IN | G_IO_HUP,
			handle_search_command_stdout_io, gsearch);
	g_io_add_watch (ioc_stderr, G_IO_IN | G_IO_HUP,
			handle_search_command_stderr_io, gsearch);

	g_io_channel_unref (ioc_stdout);
	g_io_channel_unref (ioc_stderr);
	g_strfreev (argv);
}

static CtkWidget *
create_constraint_box (GSearchWindow * gsearch,
                       GSearchConstraint * opt,
                       gchar * value)
{
	CtkWidget * hbox;
	CtkWidget * label;
	CtkWidget * entry;
	CtkWidget * entry_hbox;
	CtkWidget * button;

	hbox = ctk_box_new (CTK_ORIENTATION_HORIZONTAL, 12);

	switch (GSearchOptionTemplates[opt->constraint_id].type) {
	case SEARCH_CONSTRAINT_TYPE_BOOLEAN:
		{
			gchar * text = remove_mnemonic_character (GSearchOptionTemplates[opt->constraint_id].desc);
			gchar * desc = g_strconcat (LEFT_LABEL_SPACING, _(text), ".", NULL);
			label = ctk_label_new (desc);
			ctk_box_pack_start (CTK_BOX (hbox), label, FALSE, FALSE, 0);
			g_free (desc);
			g_free (text);
		}
		break;
	case SEARCH_CONSTRAINT_TYPE_TEXT:
	case SEARCH_CONSTRAINT_TYPE_NUMERIC:
	case SEARCH_CONSTRAINT_TYPE_DATE_BEFORE:
	case SEARCH_CONSTRAINT_TYPE_DATE_AFTER:
		{
			gchar * desc = g_strconcat (LEFT_LABEL_SPACING, _(GSearchOptionTemplates[opt->constraint_id].desc), ":", NULL);
			label = ctk_label_new_with_mnemonic (desc);
			g_free (desc);
		}

		/* add description label */
		ctk_box_pack_start (CTK_BOX (hbox), label, FALSE, FALSE, 0);

		if (GSearchOptionTemplates[opt->constraint_id].type == SEARCH_CONSTRAINT_TYPE_TEXT) {
			entry = ctk_entry_new ();
			if (value != NULL) {
				ctk_entry_set_text (CTK_ENTRY (entry), value);
				opt->data.text = value;
			}
		}
		else {
			entry = ctk_spin_button_new_with_range (0, 999999999, 1);
			if (value != NULL) {
				ctk_spin_button_set_value (CTK_SPIN_BUTTON (entry), atoi (value));
				opt->data.time = atoi (value);
				opt->data.number = atoi (value);
			}
		}

		if (gsearch->is_window_accessible) {
			gchar * text = remove_mnemonic_character (GSearchOptionTemplates[opt->constraint_id].desc);
			gchar * name;
			gchar * desc;

			if (GSearchOptionTemplates[opt->constraint_id].units == NULL) {
				name = g_strdup (_(text));
				desc = g_strdup_printf (_("Enter a text value for the \"%s\" search option."), _(text));
			}
			else {
				/* Translators:  Below is a string displaying the search options name
				and unit value.  For example, "\"Date modified less than\" in days". */
				name = g_strdup_printf (_("\"%s\" in %s"), _(text),
				                        _(GSearchOptionTemplates[opt->constraint_id].units));
				desc = g_strdup_printf (_("Enter a value in %s for the \"%s\" search option."),
				                        _(GSearchOptionTemplates[opt->constraint_id].units),
				                        _(text));
			}
			add_atk_namedesc (CTK_WIDGET (entry), name, desc);
			g_free (name);
			g_free (desc);
			g_free (text);
		}

		ctk_label_set_mnemonic_widget (CTK_LABEL (label), CTK_WIDGET (entry));

		g_signal_connect (G_OBJECT (entry), "changed",
			 	  G_CALLBACK (constraint_update_info_cb), opt);

		g_signal_connect (G_OBJECT (entry), "activate",
				  G_CALLBACK (constraint_activate_cb),
				  (gpointer) gsearch);

		/* add text field */
		entry_hbox = ctk_box_new (CTK_ORIENTATION_HORIZONTAL, 6);
		ctk_box_pack_start (CTK_BOX (hbox), entry_hbox, TRUE, TRUE, 0);
		ctk_box_pack_start (CTK_BOX (entry_hbox), entry, TRUE, TRUE, 0);

		/* add units label */
		if (GSearchOptionTemplates[opt->constraint_id].units != NULL)
		{
			label = ctk_label_new_with_mnemonic (_(GSearchOptionTemplates[opt->constraint_id].units));
			ctk_box_pack_start (CTK_BOX (entry_hbox), label, FALSE, FALSE, 0);
		}

		break;
	default:
		/* This should never happen.  If it does, there is a bug */
		label = ctk_label_new ("???");
		ctk_box_pack_start (CTK_BOX (hbox), label, TRUE, TRUE, 0);
	        break;
	}

	button = ctk_button_new_from_stock (CTK_STOCK_REMOVE);

	ctk_widget_set_can_default (button, FALSE);

	{
		GList * list = NULL;

		list = g_list_append (list, (gpointer) gsearch);
		list = g_list_append (list, (gpointer) opt);

		g_signal_connect (G_OBJECT (button), "clicked",
		                  G_CALLBACK (remove_constraint_cb),
		                  (gpointer) list);

	}
	ctk_size_group_add_widget (gsearch->available_options_button_size_group, button);
	ctk_box_pack_end (CTK_BOX (hbox), button, FALSE, FALSE, 0);

	if (gsearch->is_window_accessible) {
		gchar * text = remove_mnemonic_character (GSearchOptionTemplates[opt->constraint_id].desc);
		gchar * name = g_strdup_printf (_("Remove \"%s\""), _(text));
		gchar * desc = g_strdup_printf (_("Click to remove the \"%s\" search option."), _(text));
		add_atk_namedesc (CTK_WIDGET (button), name, desc);
		g_free (name);
		g_free (desc);
		g_free (text);
	}
	return hbox;
}

void
add_constraint (GSearchWindow * gsearch,
                gint constraint_id,
                gchar * value,
                gboolean show_constraint)
{
	GSearchConstraint * constraint = g_slice_new (GSearchConstraint);
	CtkWidget * widget;

	if (show_constraint) {
		if (ctk_widget_get_visible (gsearch->available_options_vbox) == FALSE) {
			ctk_expander_set_expanded (CTK_EXPANDER (gsearch->show_more_options_expander), TRUE);
			ctk_widget_show (gsearch->available_options_vbox);
		}
	}

	gsearch->window_geometry.min_height += WINDOW_HEIGHT_STEP;

	if (ctk_widget_get_visible (gsearch->available_options_vbox)) {
		ctk_window_set_geometry_hints (CTK_WINDOW (gsearch->window),
		                               CTK_WIDGET (gsearch->window),
		                               &gsearch->window_geometry,
		                               CDK_HINT_MIN_SIZE);
	}

	constraint->constraint_id = constraint_id;
	set_constraint_info_defaults (constraint);
	set_constraint_gsettings_boolean (constraint->constraint_id, TRUE);

	widget = create_constraint_box (gsearch, constraint, value);
	ctk_box_pack_start (CTK_BOX (gsearch->available_options_vbox), widget, FALSE, FALSE, 0);
	ctk_widget_show_all (widget);

	gsearch->available_options_selected_list =
		g_list_append (gsearch->available_options_selected_list, constraint);

	set_constraint_selected_state (gsearch, constraint->constraint_id, TRUE);
}

static void
set_sensitive (CtkCellLayout * cell_layout,
               CtkCellRenderer * cell,
               CtkTreeModel * tree_model,
               CtkTreeIter * iter,
               gpointer data)
{
	CtkTreePath * path;
	gint idx;

	path = ctk_tree_model_get_path (tree_model, iter);
	idx = ctk_tree_path_get_indices (path)[0];
	ctk_tree_path_free (path);

	g_object_set (cell, "sensitive", !(GSearchOptionTemplates[idx].is_selected), NULL);
}

static gboolean
is_separator (CtkTreeModel * model,
              CtkTreeIter * iter,
              gpointer data)
{
	CtkTreePath * path;
	gint idx;

	path = ctk_tree_model_get_path (model, iter);
	idx = ctk_tree_path_get_indices (path)[0];
	ctk_tree_path_free (path);

	return (GSearchOptionTemplates[idx].type == SEARCH_CONSTRAINT_TYPE_SEPARATOR);
}

static void
create_additional_constraint_section (GSearchWindow * gsearch)
{
	CtkCellRenderer * renderer;
	CtkTreeModel * model;
	CtkWidget * hbox;
	gchar * desc;

	gsearch->available_options_vbox = ctk_box_new (CTK_ORIENTATION_VERTICAL, 6);
	hbox = ctk_box_new (CTK_ORIENTATION_HORIZONTAL, 12);

	ctk_box_pack_end (CTK_BOX (gsearch->available_options_vbox), hbox, FALSE, FALSE, 0);

	desc = g_strconcat (LEFT_LABEL_SPACING, _("A_vailable options:"), NULL);
	gsearch->available_options_label = ctk_label_new_with_mnemonic (desc);
	g_free (desc);

	ctk_box_pack_start (CTK_BOX (hbox), gsearch->available_options_label, FALSE, FALSE, 0);

	model = gsearch_create_list_of_templates ();
	gsearch->available_options_combo_box = ctk_combo_box_new_with_model (model);
	g_object_unref (model);

	ctk_label_set_mnemonic_widget (CTK_LABEL (gsearch->available_options_label), CTK_WIDGET (gsearch->available_options_combo_box));
	ctk_combo_box_set_active (CTK_COMBO_BOX (gsearch->available_options_combo_box), 0);
	ctk_box_pack_start (CTK_BOX (hbox), gsearch->available_options_combo_box, TRUE, TRUE, 0);

	renderer = ctk_cell_renderer_text_new ();
	ctk_cell_layout_pack_start (CTK_CELL_LAYOUT (gsearch->available_options_combo_box),
	                            renderer,
	                            TRUE);
	ctk_cell_layout_set_attributes (CTK_CELL_LAYOUT (gsearch->available_options_combo_box), renderer,
	                                "text", 0,
	                                NULL);
	ctk_cell_layout_set_cell_data_func (CTK_CELL_LAYOUT (gsearch->available_options_combo_box),
	                                    renderer,
	                                    set_sensitive,
	                                    NULL, NULL);
	ctk_combo_box_set_row_separator_func (CTK_COMBO_BOX (gsearch->available_options_combo_box),
	                                      is_separator, NULL, NULL);

	if (gsearch->is_window_accessible) {
		add_atk_namedesc (CTK_WIDGET (gsearch->available_options_combo_box), _("Available options"),
				  _("Select a search option from the drop-down list."));
	}

	gsearch->available_options_add_button = ctk_button_new_from_stock (CTK_STOCK_ADD);

	ctk_widget_set_can_default (gsearch->available_options_add_button, FALSE);
	gsearch->available_options_button_size_group = ctk_size_group_new (CTK_SIZE_GROUP_BOTH);
	ctk_size_group_add_widget (gsearch->available_options_button_size_group, gsearch->available_options_add_button);

	g_signal_connect (G_OBJECT (gsearch->available_options_add_button),"clicked",
			  G_CALLBACK (add_constraint_cb), (gpointer) gsearch);

	if (gsearch->is_window_accessible) {
		add_atk_namedesc (CTK_WIDGET (gsearch->available_options_add_button), _("Add search option"),
				  _("Click to add the selected available search option."));
	}

	ctk_box_pack_end (CTK_BOX (hbox), gsearch->available_options_add_button, FALSE, FALSE, 0);
}

static void
filename_cell_data_func (CtkTreeViewColumn * column,
                         CtkCellRenderer * renderer,
                         CtkTreeModel * model,
                         CtkTreeIter * iter,
                         GSearchWindow * gsearch)
{
	CtkTreePath * path;
	PangoUnderline underline;
	gboolean underline_set;

	if (gsearch->is_search_results_single_click_to_activate == TRUE) {

		path = ctk_tree_model_get_path (model, iter);

		if ((gsearch->search_results_hover_path == NULL) ||
		    (ctk_tree_path_compare (path, gsearch->search_results_hover_path) != 0)) {
			underline = PANGO_UNDERLINE_NONE;
			underline_set = FALSE;
		}
		else {
			underline = PANGO_UNDERLINE_SINGLE;
			underline_set = TRUE;
		}
		ctk_tree_path_free (path);
	}
	else {
		underline = PANGO_UNDERLINE_NONE;
		underline_set = FALSE;
	}

	g_object_set (gsearch->search_results_name_cell_renderer,
	              "underline", underline,
	              "underline-set", underline_set,
	              NULL);
}

static gboolean
gsearch_equal_func (CtkTreeModel * model,
                    gint column,
                    const gchar * key,
                    CtkTreeIter * iter,
                    gpointer search_data)
{
	gboolean results = TRUE;
	gchar * name;

	ctk_tree_model_get (model, iter, COLUMN_NAME, &name, -1);

	if (name != NULL) {
		gchar * casefold_key;
		gchar * casefold_name;

		casefold_key = g_utf8_casefold (key, -1);
		casefold_name = g_utf8_casefold (name, -1);

		if ((casefold_key != NULL) &&
		    (casefold_name != NULL) &&
		    (strstr (casefold_name, casefold_key) != NULL)) {
			results = FALSE;
		}
		g_free (casefold_key);
		g_free (casefold_name);
		g_free (name);
	}
	return results;
}

static CtkWidget *
create_search_results_section (GSearchWindow * gsearch)
{
	CtkWidget * label;
	CtkWidget * vbox;
	CtkWidget * hbox;
	CtkWidget * window;
	CtkTreeViewColumn * column;
	CtkCellRenderer * renderer;

	vbox = ctk_box_new (CTK_ORIENTATION_VERTICAL, 6);
	hbox = ctk_box_new (CTK_ORIENTATION_HORIZONTAL, 6);

	ctk_box_pack_start (CTK_BOX (vbox), hbox, FALSE, FALSE, 0);

	label = ctk_label_new_with_mnemonic (_("S_earch results:"));
	g_object_set (G_OBJECT (label), "xalign", 0.0, NULL);
	ctk_box_pack_start (CTK_BOX (hbox), label, FALSE, TRUE, 0);

	gsearch->progress_spinner = ctk_spinner_new ();
	ctk_widget_set_size_request (gsearch->progress_spinner,
                                     CTK_ICON_SIZE_MENU, CTK_ICON_SIZE_MENU);
	ctk_box_pack_start (CTK_BOX (hbox), gsearch->progress_spinner, FALSE, FALSE, 0);

	gsearch->files_found_label = ctk_label_new (NULL);
	ctk_label_set_selectable (CTK_LABEL (gsearch->files_found_label), TRUE);
	g_object_set (G_OBJECT (gsearch->files_found_label), "xalign", 1.0, NULL);
	ctk_box_pack_start (CTK_BOX (hbox), gsearch->files_found_label, TRUE, TRUE, 0);

	window = ctk_scrolled_window_new (NULL, NULL);
	ctk_scrolled_window_set_shadow_type (CTK_SCROLLED_WINDOW (window), CTK_SHADOW_IN);
	ctk_container_set_border_width (CTK_CONTAINER (window), 0);
	ctk_widget_set_size_request (window, 530, 160);
	ctk_scrolled_window_set_policy (CTK_SCROLLED_WINDOW (window),
                                        CTK_POLICY_AUTOMATIC,
                                        CTK_POLICY_AUTOMATIC);

	gsearch->search_results_list_store = ctk_list_store_new (NUM_COLUMNS,
					      GDK_TYPE_PIXBUF,
					      G_TYPE_STRING,
					      G_TYPE_STRING,
					      G_TYPE_STRING,
					      G_TYPE_STRING,
					      G_TYPE_DOUBLE,
					      G_TYPE_STRING,
					      G_TYPE_STRING,
					      G_TYPE_DOUBLE,
					      G_TYPE_POINTER,
					      G_TYPE_BOOLEAN);

	gsearch->search_results_tree_view = CTK_TREE_VIEW (ctk_tree_view_new_with_model (CTK_TREE_MODEL (gsearch->search_results_list_store)));

	ctk_tree_view_set_headers_visible (gsearch->search_results_tree_view, FALSE);
	ctk_tree_view_set_search_equal_func (gsearch->search_results_tree_view,
	                                     gsearch_equal_func, NULL, NULL);
  	g_object_unref (G_OBJECT (gsearch->search_results_list_store));

	if (gsearch->is_window_accessible) {
		add_atk_namedesc (CTK_WIDGET (gsearch->search_results_tree_view), _("List View"), NULL);
	}

	gsearch->search_results_selection = ctk_tree_view_get_selection (CTK_TREE_VIEW (gsearch->search_results_tree_view));

	ctk_tree_selection_set_mode (CTK_TREE_SELECTION (gsearch->search_results_selection),
				     CTK_SELECTION_MULTIPLE);

	ctk_drag_source_set (CTK_WIDGET (gsearch->search_results_tree_view),
			     CDK_BUTTON1_MASK | CDK_BUTTON2_MASK,
			     GSearchDndTable, GSearchTotalDnds,
			     CDK_ACTION_COPY | CDK_ACTION_MOVE | CDK_ACTION_ASK);

	g_signal_connect (G_OBJECT (gsearch->search_results_tree_view),
			  "drag_data_get",
			  G_CALLBACK (drag_file_cb),
			  (gpointer) gsearch);

	g_signal_connect (G_OBJECT (gsearch->search_results_tree_view),
			  "drag_begin",
			  G_CALLBACK (drag_begin_file_cb),
			  (gpointer) gsearch);

	g_signal_connect (G_OBJECT (gsearch->search_results_tree_view),
			  "event_after",
		          G_CALLBACK (file_event_after_cb),
			  (gpointer) gsearch);

	g_signal_connect (G_OBJECT (gsearch->search_results_tree_view),
			  "button_release_event",
		          G_CALLBACK (file_button_release_event_cb),
			  (gpointer) gsearch);

	g_signal_connect (G_OBJECT (gsearch->search_results_tree_view),
			  "button_press_event",
		          G_CALLBACK (file_button_press_event_cb),
			  (gpointer) gsearch->search_results_tree_view);

	g_signal_connect (G_OBJECT (gsearch->search_results_tree_view),
			  "key_press_event",
			  G_CALLBACK (file_key_press_event_cb),
			  (gpointer) gsearch);

	g_signal_connect (G_OBJECT (gsearch->search_results_tree_view),
	                  "motion_notify_event",
	                  G_CALLBACK (file_motion_notify_cb),
	                  (gpointer) gsearch);

	g_signal_connect (G_OBJECT (gsearch->search_results_tree_view),
	                  "leave_notify_event",
	                  G_CALLBACK (file_leave_notify_cb),
	                  (gpointer) gsearch);

	ctk_label_set_mnemonic_widget (CTK_LABEL (label), CTK_WIDGET (gsearch->search_results_tree_view));

	ctk_container_add (CTK_CONTAINER (window), CTK_WIDGET (gsearch->search_results_tree_view));
	ctk_box_pack_end (CTK_BOX (vbox), window, TRUE, TRUE, 0);

	/* create the name column */
	column = ctk_tree_view_column_new ();
	ctk_tree_view_column_set_title (column, _("Name"));

	renderer = ctk_cell_renderer_pixbuf_new ();
	ctk_tree_view_column_pack_start (column, renderer, FALSE);
        ctk_tree_view_column_set_attributes (column, renderer,
                                             "pixbuf", COLUMN_ICON,
                                             NULL);

	gsearch->search_results_name_cell_renderer = ctk_cell_renderer_text_new ();
        ctk_tree_view_column_pack_start (column, gsearch->search_results_name_cell_renderer, TRUE);
        ctk_tree_view_column_set_attributes (column, gsearch->search_results_name_cell_renderer,
                                             "text", COLUMN_NAME,
					     NULL);
	ctk_tree_view_column_set_sizing (column, CTK_TREE_VIEW_COLUMN_AUTOSIZE);
	ctk_tree_view_column_set_resizable (column, TRUE);
	ctk_tree_view_column_set_sort_column_id (column, COLUMN_NAME);
	ctk_tree_view_column_set_reorderable (column, TRUE);
	ctk_tree_view_column_set_cell_data_func (column, renderer,
	                                         (CtkTreeCellDataFunc) filename_cell_data_func,
						 gsearch, NULL);
	ctk_tree_view_append_column (CTK_TREE_VIEW (gsearch->search_results_tree_view), column);

	/* create the folder column */
	renderer = ctk_cell_renderer_text_new ();
	column = ctk_tree_view_column_new_with_attributes (_("Folder"), renderer,
							   "text", COLUMN_RELATIVE_PATH,
							   NULL);
	ctk_tree_view_column_set_sizing (column, CTK_TREE_VIEW_COLUMN_AUTOSIZE);
	ctk_tree_view_column_set_resizable (column, TRUE);
	ctk_tree_view_column_set_sort_column_id (column, COLUMN_RELATIVE_PATH);
	ctk_tree_view_column_set_reorderable (column, TRUE);
	ctk_tree_view_append_column (CTK_TREE_VIEW (gsearch->search_results_tree_view), column);
	gsearch->search_results_folder_column = column;

	/* create the size column */
	renderer = ctk_cell_renderer_text_new ();
	g_object_set (renderer, "xalign", 1.0, NULL);
	column = ctk_tree_view_column_new_with_attributes (_("Size"), renderer,
							   "text", COLUMN_READABLE_SIZE,
							   NULL);
	ctk_tree_view_column_set_sizing (column, CTK_TREE_VIEW_COLUMN_AUTOSIZE);
	ctk_tree_view_column_set_resizable (column, TRUE);
	ctk_tree_view_column_set_sort_column_id (column, COLUMN_SIZE);
	ctk_tree_view_column_set_reorderable (column, TRUE);
	ctk_tree_view_append_column (CTK_TREE_VIEW (gsearch->search_results_tree_view), column);
	gsearch->search_results_size_column = column;

	/* create the type column */
	renderer = ctk_cell_renderer_text_new ();
	column = ctk_tree_view_column_new_with_attributes (_("Type"), renderer,
							   "text", COLUMN_TYPE,
							   NULL);
	ctk_tree_view_column_set_sizing (column, CTK_TREE_VIEW_COLUMN_AUTOSIZE);
	ctk_tree_view_column_set_resizable (column, TRUE);
	ctk_tree_view_column_set_sort_column_id (column, COLUMN_TYPE);
	ctk_tree_view_column_set_reorderable (column, TRUE);
	ctk_tree_view_append_column (CTK_TREE_VIEW (gsearch->search_results_tree_view), column);
	gsearch->search_results_type_column = column;

	/* create the date modified column */
	renderer = ctk_cell_renderer_text_new ();
	column = ctk_tree_view_column_new_with_attributes (_("Date Modified"), renderer,
							   "text", COLUMN_READABLE_DATE,
							   NULL);
	ctk_tree_view_column_set_sizing (column, CTK_TREE_VIEW_COLUMN_AUTOSIZE);
	ctk_tree_view_column_set_resizable (column, TRUE);
	ctk_tree_view_column_set_sort_column_id (column, COLUMN_DATE);
	ctk_tree_view_column_set_reorderable (column, TRUE);
	ctk_tree_view_append_column (CTK_TREE_VIEW (gsearch->search_results_tree_view), column);
	gsearch->search_results_date_column = column;

	gsearchtool_set_columns_order (gsearch->search_results_tree_view);

	g_signal_connect (G_OBJECT (gsearch->search_results_tree_view),
	                  "columns-changed",
	                  G_CALLBACK (columns_changed_cb),
	                  (gpointer) gsearch);
	return vbox;
}

static void
register_gsearchtool_icon (CtkIconFactory * factory)
{
	CtkIconSource * source;
	CtkIconSet * icon_set;

	source = ctk_icon_source_new ();

	ctk_icon_source_set_icon_name (source, CAFE_SEARCH_TOOL_ICON);

	icon_set = ctk_icon_set_new ();
	ctk_icon_set_add_source (icon_set, source);

	ctk_icon_factory_add (factory, CAFE_SEARCH_TOOL_STOCK, icon_set);

	ctk_icon_set_unref (icon_set);

	ctk_icon_source_free (source);
}

static void
gsearchtool_init_stock_icons (void)
{
	CtkIconFactory * factory;

	factory = ctk_icon_factory_new ();
	ctk_icon_factory_add_default (factory);

	register_gsearchtool_icon (factory);

	g_object_unref (factory);
}

void
set_clone_command (GSearchWindow * gsearch,
                   gint * argcp,
                   gchar *** argvp,
                   gpointer client_data,
                   gboolean escape_values)
{
	gchar ** argv;
	gchar * file_is_named_utf8;
	gchar * file_is_named_locale;
	gchar * look_in_folder_locale;
	gchar * tmp;
	GList * list;
	gint  i = 0;

	argv = g_new0 (gchar*, SEARCH_CONSTRAINT_MAXIMUM_POSSIBLE);

	argv[i++] = (gchar *) client_data;

	file_is_named_utf8 = (gchar *) ctk_entry_get_text (CTK_ENTRY (gsearch_history_entry_get_entry (GSEARCH_HISTORY_ENTRY (gsearch->name_contains_entry))));
	file_is_named_locale = g_locale_from_utf8 (file_is_named_utf8 != NULL ? file_is_named_utf8 : "" ,
	                                           -1, NULL, NULL, NULL);
	if (escape_values)
		tmp = g_shell_quote (file_is_named_locale);
	else
		tmp = g_strdup (file_is_named_locale);
	argv[i++] = g_strdup_printf ("--named=%s", tmp);
	g_free (tmp);
	g_free (file_is_named_locale);

	look_in_folder_locale = ctk_file_chooser_get_current_folder (CTK_FILE_CHOOSER (gsearch->look_in_folder_button));

	if (look_in_folder_locale == NULL) {
		look_in_folder_locale = g_strdup ("");
	}

	if (escape_values)
		tmp = g_shell_quote (look_in_folder_locale);
	else
		tmp = g_strdup (look_in_folder_locale);
	argv[i++] = g_strdup_printf ("--path=%s", tmp);
	g_free (tmp);
	g_free (look_in_folder_locale);

	if (ctk_widget_get_visible (gsearch->available_options_vbox)) {
		for (list = gsearch->available_options_selected_list; list != NULL; list = g_list_next (list)) {
			GSearchConstraint * constraint = list->data;
			gchar * locale = NULL;

			switch (constraint->constraint_id) {
			case SEARCH_CONSTRAINT_CONTAINS_THE_TEXT:
				locale = g_locale_from_utf8 (constraint->data.text, -1, NULL, NULL, NULL);
				if (escape_values)
					tmp = g_shell_quote (locale);
				else
					tmp = g_strdup (locale);
				argv[i++] = g_strdup_printf ("--contains=%s", tmp);
				g_free (tmp);
				break;
			case SEARCH_CONSTRAINT_DATE_MODIFIED_BEFORE:
				argv[i++] = g_strdup_printf ("--mtimeless=%d", constraint->data.time);
				break;
			case SEARCH_CONSTRAINT_DATE_MODIFIED_AFTER:
				argv[i++] = g_strdup_printf ("--mtimemore=%d", constraint->data.time);
				break;
			case SEARCH_CONSTRAINT_SIZE_IS_MORE_THAN:
				argv[i++] = g_strdup_printf ("--sizemore=%u", constraint->data.number);
				break;
			case SEARCH_CONSTRAINT_SIZE_IS_LESS_THAN:
				argv[i++] = g_strdup_printf ("--sizeless=%u", constraint->data.number);
				break;
			case SEARCH_CONSTRAINT_FILE_IS_EMPTY:
				argv[i++] = g_strdup ("--empty");
				break;
			case SEARCH_CONSTRAINT_OWNED_BY_USER:
				locale = g_locale_from_utf8 (constraint->data.text, -1, NULL, NULL, NULL);
				if (escape_values)
					tmp = g_shell_quote (locale);
				else
					tmp = g_strdup (locale);
				argv[i++] = g_strdup_printf ("--user=%s", tmp);
				g_free (tmp);
				break;
			case SEARCH_CONSTRAINT_OWNED_BY_GROUP:
				locale = g_locale_from_utf8 (constraint->data.text, -1, NULL, NULL, NULL);
				if (escape_values)
					tmp = g_shell_quote (locale);
				else
					tmp = g_strdup (locale);
				argv[i++] = g_strdup_printf ("--group=%s", tmp);
				g_free (tmp);
				break;
			case SEARCH_CONSTRAINT_OWNER_IS_UNRECOGNIZED:
				argv[i++] = g_strdup ("--nouser");
				break;
			case SEARCH_CONSTRAINT_FILE_IS_NOT_NAMED:
				locale = g_locale_from_utf8 (constraint->data.text, -1, NULL, NULL, NULL);
				if (escape_values)
					tmp = g_shell_quote (locale);
				else
					tmp = g_strdup (locale);
				argv[i++] = g_strdup_printf ("--notnamed=%s", tmp);
				g_free (tmp);
				break;
			case SEARCH_CONSTRAINT_FILE_MATCHES_REGULAR_EXPRESSION:
				locale = g_locale_from_utf8 (constraint->data.text, -1, NULL, NULL, NULL);
				if (escape_values)
					tmp = g_shell_quote (locale);
				else
					tmp = g_strdup (locale);
				argv[i++] = g_strdup_printf ("--regex=%s", tmp);
				g_free (tmp);
				break;
			case SEARCH_CONSTRAINT_SHOW_HIDDEN_FILES_AND_FOLDERS:
				argv[i++] = g_strdup ("--hidden");
				break;
			case SEARCH_CONSTRAINT_FOLLOW_SYMBOLIC_LINKS:
				argv[i++] = g_strdup ("--follow");
				break;
			case SEARCH_CONSTRAINT_SEARCH_OTHER_FILESYSTEMS:
				argv[i++] = g_strdup ("--mounts");
				break;
			default:
				break;
			}
			g_free (locale);
		}
	}
	*argvp = argv;
	*argcp = i;
}

static void
handle_gsettings_settings (GSearchWindow * gsearch)
{
	if (g_settings_get_boolean (gsearch->cafe_search_tool_settings, "show-additional-options")) {
		if (ctk_widget_get_visible (gsearch->available_options_vbox) == FALSE) {
			ctk_expander_set_expanded (CTK_EXPANDER (gsearch->show_more_options_expander), TRUE);
			ctk_widget_show (gsearch->available_options_vbox);
		}
	}

	if (g_settings_get_boolean (gsearch->cafe_search_tool_select_settings, "contains-the-text")) {
		add_constraint (gsearch, SEARCH_CONSTRAINT_CONTAINS_THE_TEXT, "", FALSE);
	}

	if (g_settings_get_boolean (gsearch->cafe_search_tool_select_settings, "date-modified-less-than")) {
		add_constraint (gsearch, SEARCH_CONSTRAINT_DATE_MODIFIED_BEFORE, "", FALSE);
	}

	if (g_settings_get_boolean (gsearch->cafe_search_tool_select_settings, "date-modified-more-than")) {
		add_constraint (gsearch, SEARCH_CONSTRAINT_DATE_MODIFIED_AFTER, "", FALSE);
	}

	if (g_settings_get_boolean (gsearch->cafe_search_tool_select_settings, "size-at-least")) {
		add_constraint (gsearch, SEARCH_CONSTRAINT_SIZE_IS_MORE_THAN, "", FALSE);
	}

	if (g_settings_get_boolean (gsearch->cafe_search_tool_select_settings, "size-at-most")) {
		add_constraint (gsearch, SEARCH_CONSTRAINT_SIZE_IS_LESS_THAN, "", FALSE);
	}

	if (g_settings_get_boolean (gsearch->cafe_search_tool_select_settings, "file-is-empty")) {
		add_constraint (gsearch, SEARCH_CONSTRAINT_FILE_IS_EMPTY, NULL, FALSE);
	}

	if (g_settings_get_boolean (gsearch->cafe_search_tool_select_settings, "owned-by-user")) {
		add_constraint (gsearch, SEARCH_CONSTRAINT_OWNED_BY_USER, "", FALSE);
	}

	if (g_settings_get_boolean (gsearch->cafe_search_tool_select_settings, "owned-by-group")) {
		add_constraint (gsearch, SEARCH_CONSTRAINT_OWNED_BY_GROUP, "", FALSE);
	}

	if (g_settings_get_boolean (gsearch->cafe_search_tool_select_settings, "owner-is-unrecognized")) {
		add_constraint (gsearch, SEARCH_CONSTRAINT_OWNER_IS_UNRECOGNIZED, NULL, FALSE);
	}

	if (g_settings_get_boolean (gsearch->cafe_search_tool_select_settings, "name-does-not-contain")) {
		add_constraint (gsearch, SEARCH_CONSTRAINT_FILE_IS_NOT_NAMED, "", FALSE);
	}

	if (g_settings_get_boolean (gsearch->cafe_search_tool_select_settings, "name-matches-regular-expression")) {
		add_constraint (gsearch, SEARCH_CONSTRAINT_FILE_MATCHES_REGULAR_EXPRESSION, "", FALSE);
	}

	if (g_settings_get_boolean (gsearch->cafe_search_tool_select_settings, "show-hidden-files-and-folders")) {
		add_constraint (gsearch, SEARCH_CONSTRAINT_SHOW_HIDDEN_FILES_AND_FOLDERS, NULL, FALSE);
	}

	if (g_settings_get_boolean (gsearch->cafe_search_tool_select_settings, "follow-symbolic-links")) {
		add_constraint (gsearch, SEARCH_CONSTRAINT_FOLLOW_SYMBOLIC_LINKS, NULL, FALSE);
	}

	if (g_settings_get_boolean (gsearch->cafe_search_tool_select_settings, "exclude-other-filesystems")) {
		add_constraint (gsearch, SEARCH_CONSTRAINT_SEARCH_OTHER_FILESYSTEMS, NULL, FALSE);
	}
}

static void
gsearch_window_size_allocate (CtkWidget * widget,
                              CtkAllocation * allocation,
                              GSearchWindow * gsearch)
{
	if (gsearch->is_window_maximized == FALSE) {
		gsearch->window_width = allocation->width;
		gsearch->window_height = allocation->height;
	}
}

static CtkWidget *
gsearch_app_create (GSearchWindow * gsearch)
{
	gchar * locale_string;
	gchar * utf8_string;
	CtkWidget * hbox;
	CtkWidget * vbox;
	CtkWidget * entry;
	CtkWidget * label;
	CtkWidget * button;
	CtkWidget * container;

	gsearch->cafe_search_tool_settings = g_settings_new ("org.cafe.search-tool");
	gsearch->cafe_search_tool_select_settings = g_settings_new ("org.cafe.search-tool.select");
	gsearch->cafe_desktop_interface_settings = g_settings_new ("org.cafe.interface");

	/* Check if baul schema is installed before trying to read baul settings */
	gsearch->baul_schema_exists = FALSE;
	GSettingsSchema *schema = g_settings_schema_source_lookup (g_settings_schema_source_get_default (),
								   BAUL_PREFERENCES_SCHEMA,
								   FALSE);

	if (schema != NULL) {
		gsearch->baul_schema_exists = TRUE;
		g_settings_schema_unref (schema);
	}

	if (gsearch->baul_schema_exists) {
		gsearch->baul_settings = g_settings_new (BAUL_PREFERENCES_SCHEMA);
	} else {
		gsearch->baul_settings = NULL;
	}

	gsearch->window = ctk_window_new (CTK_WINDOW_TOPLEVEL);
	gsearch->is_window_maximized = g_settings_get_boolean (gsearch->cafe_search_tool_settings, "default-window-maximized");
	g_signal_connect (G_OBJECT (gsearch->window), "size-allocate",
			  G_CALLBACK (gsearch_window_size_allocate),
			  gsearch);
	gsearch->command_details = g_slice_new0 (GSearchCommandDetails);
	gsearch->window_geometry.min_height = MINIMUM_WINDOW_HEIGHT;
	gsearch->window_geometry.min_width  = MINIMUM_WINDOW_WIDTH;

	ctk_window_set_position (CTK_WINDOW (gsearch->window), CTK_WIN_POS_CENTER);
	ctk_window_set_geometry_hints (CTK_WINDOW (gsearch->window), CTK_WIDGET (gsearch->window),
				       &gsearch->window_geometry, CDK_HINT_MIN_SIZE);

	gsearchtool_get_stored_window_geometry (&gsearch->window_width,
	                                        &gsearch->window_height);
	ctk_window_set_default_size (CTK_WINDOW (gsearch->window),
	                             gsearch->window_width,
	                             gsearch->window_height);

	if (gsearch->is_window_maximized == TRUE) {
		ctk_window_maximize (CTK_WINDOW (gsearch->window));
	}

	container = ctk_box_new (CTK_ORIENTATION_VERTICAL, 6);
	ctk_container_add (CTK_CONTAINER (gsearch->window), container);
	ctk_container_set_border_width (CTK_CONTAINER (container), 12);

	hbox = ctk_box_new (CTK_ORIENTATION_HORIZONTAL, 6);
	ctk_box_pack_start (CTK_BOX (container), hbox, FALSE, FALSE, 0);

	gsearch->name_and_folder_table = ctk_grid_new ();
	ctk_grid_set_row_spacing (CTK_GRID (gsearch->name_and_folder_table), 6);
	ctk_grid_set_column_spacing (CTK_GRID (gsearch->name_and_folder_table), 12);
	ctk_container_add (CTK_CONTAINER (hbox), gsearch->name_and_folder_table);

	label = ctk_label_new_with_mnemonic (_("_Name contains:"));
	ctk_label_set_justify (CTK_LABEL (label), CTK_JUSTIFY_LEFT);
	g_object_set (G_OBJECT (label), "xalign", 0.0, NULL);

	ctk_grid_attach (CTK_GRID (gsearch->name_and_folder_table), label, 0, 0, 1, 1);

	gsearch->name_contains_entry = gsearch_history_entry_new ("gsearchtool-file-entry", FALSE);
	ctk_label_set_mnemonic_widget (CTK_LABEL (label), gsearch->name_contains_entry);
	gsearch_history_entry_set_history_length (GSEARCH_HISTORY_ENTRY (gsearch->name_contains_entry), 10);
	ctk_widget_set_hexpand (gsearch->name_contains_entry, TRUE);
	ctk_grid_attach (CTK_GRID (gsearch->name_and_folder_table), gsearch->name_contains_entry, 1, 0, 1, 1);
	entry =  gsearch_history_entry_get_entry (GSEARCH_HISTORY_ENTRY (gsearch->name_contains_entry));

	if (CTK_IS_ACCESSIBLE (ctk_widget_get_accessible (gsearch->name_contains_entry))) {
		gsearch->is_window_accessible = TRUE;
		add_atk_namedesc (gsearch->name_contains_entry, NULL, _("Enter a filename or partial filename with or without wildcards."));
		add_atk_namedesc (entry, _("Name contains"), _("Enter a filename or partial filename with or without wildcards."));
	}
	g_signal_connect (G_OBJECT (gsearch_history_entry_get_entry (GSEARCH_HISTORY_ENTRY (gsearch->name_contains_entry))), "activate",
			  G_CALLBACK (name_contains_activate_cb),
			  (gpointer) gsearch);

	label = ctk_label_new_with_mnemonic (_("_Look in folder:"));
	ctk_label_set_justify (CTK_LABEL (label), CTK_JUSTIFY_LEFT);
	g_object_set (G_OBJECT (label), "xalign", 0.0, NULL);

	ctk_grid_attach (CTK_GRID (gsearch->name_and_folder_table), label, 0, 1, 1, 1);

	gsearch->look_in_folder_button = ctk_file_chooser_button_new (_("Browse"), CTK_FILE_CHOOSER_ACTION_SELECT_FOLDER);
	ctk_label_set_mnemonic_widget (CTK_LABEL (label), CTK_WIDGET (gsearch->look_in_folder_button));
	ctk_widget_set_hexpand (gsearch->look_in_folder_button, TRUE);
	ctk_grid_attach (CTK_GRID (gsearch->name_and_folder_table), gsearch->look_in_folder_button, 1, 1, 1, 1);

	g_signal_connect (G_OBJECT (gsearch->look_in_folder_button), "current-folder-changed",
	                  G_CALLBACK (look_in_folder_changed_cb),
			  (gpointer) gsearch);

	if (gsearch->is_window_accessible) {
		add_atk_namedesc (CTK_WIDGET (gsearch->look_in_folder_button), _("Look in folder"), _("Select the folder or device from which you want to begin the search."));
	}

	locale_string = g_settings_get_string (gsearch->cafe_search_tool_settings, "look-in-folder");

	if ((g_file_test (locale_string, G_FILE_TEST_EXISTS) == FALSE) ||
	    (g_file_test (locale_string, G_FILE_TEST_IS_DIR) == FALSE)) {
		g_free (locale_string);
		locale_string = g_get_current_dir ();
	}

	utf8_string = g_filename_to_utf8 (locale_string, -1, NULL, NULL, NULL);

	ctk_file_chooser_set_current_folder (CTK_FILE_CHOOSER (gsearch->look_in_folder_button), utf8_string);

	g_free (locale_string);
	g_free (utf8_string);

	gsearch->show_more_options_expander = ctk_expander_new_with_mnemonic (_("Select more _options"));
	ctk_box_pack_start (CTK_BOX (container), gsearch->show_more_options_expander, FALSE, FALSE, 0);
	g_signal_connect (G_OBJECT (gsearch->show_more_options_expander), "notify::expanded",
			  G_CALLBACK (click_expander_cb), (gpointer) gsearch);

	create_additional_constraint_section (gsearch);
	ctk_box_pack_start (CTK_BOX (container), CTK_WIDGET (gsearch->available_options_vbox), FALSE, FALSE, 0);

	if (gsearch->is_window_accessible) {
		add_atk_namedesc (CTK_WIDGET (gsearch->show_more_options_expander), _("Select more options"), _("Click to expand or collapse the list of available options."));
		add_atk_relation (CTK_WIDGET (gsearch->available_options_vbox), CTK_WIDGET (gsearch->show_more_options_expander), ATK_RELATION_CONTROLLED_BY);
		add_atk_relation (CTK_WIDGET (gsearch->show_more_options_expander), CTK_WIDGET (gsearch->available_options_vbox), ATK_RELATION_CONTROLLER_FOR);
	}

	vbox = ctk_box_new (CTK_ORIENTATION_VERTICAL, 12);
	ctk_box_pack_start (CTK_BOX (container), vbox, TRUE, TRUE, 0);

	gsearch->search_results_vbox = create_search_results_section (gsearch);
	ctk_widget_set_sensitive (CTK_WIDGET (gsearch->search_results_vbox), FALSE);
	ctk_box_pack_start (CTK_BOX (vbox), gsearch->search_results_vbox, TRUE, TRUE, 0);

	hbox = ctk_button_box_new (CTK_ORIENTATION_HORIZONTAL);
	ctk_button_box_set_layout (CTK_BUTTON_BOX (hbox), CTK_BUTTONBOX_END);
	ctk_box_pack_start (CTK_BOX (vbox), hbox, FALSE, FALSE, 0);

	ctk_box_set_spacing (CTK_BOX (hbox), 6);

	button = ctk_button_new_from_stock (CTK_STOCK_HELP);

	ctk_widget_set_can_default (button, TRUE);
	ctk_box_pack_start (CTK_BOX (hbox), button, FALSE, FALSE, 0);
	ctk_button_box_set_child_secondary (CTK_BUTTON_BOX (hbox), button, TRUE);
	g_signal_connect (G_OBJECT (button), "clicked",
			  G_CALLBACK (click_help_cb), (gpointer) gsearch->window);
  	if (gsearch->is_window_accessible) {
		add_atk_namedesc (CTK_WIDGET (button), NULL, _("Click to display the help manual."));
	}

	button = ctk_button_new_from_stock (CTK_STOCK_CLOSE);

	ctk_widget_set_can_default (button, TRUE);
	g_signal_connect (G_OBJECT (button), "clicked",
			  G_CALLBACK (click_close_cb), (gpointer) gsearch);
  	if (gsearch->is_window_accessible) {
		add_atk_namedesc (CTK_WIDGET (button), NULL, _("Click to close \"Search for Files\"."));
	}

	ctk_box_pack_start (CTK_BOX (hbox), button, FALSE, FALSE, 0);

	/* Find and Stop buttons... */
	gsearch->find_button = ctk_button_new_from_stock (CTK_STOCK_FIND);
	gsearch->stop_button = ctk_button_new_from_stock (CTK_STOCK_STOP);

	ctk_widget_set_can_default (gsearch->find_button, TRUE);
	ctk_widget_set_can_default (gsearch->stop_button, TRUE);

	ctk_box_pack_end (CTK_BOX (hbox), gsearch->stop_button, FALSE, FALSE, 0);
	ctk_box_pack_end (CTK_BOX (hbox), gsearch->find_button, FALSE, FALSE, 0);

	ctk_widget_set_sensitive (gsearch->stop_button, FALSE);
	ctk_widget_set_sensitive (gsearch->find_button, TRUE);

	g_signal_connect (G_OBJECT (gsearch->find_button), "clicked",
	                  G_CALLBACK (click_find_cb), (gpointer) gsearch);
    	g_signal_connect (G_OBJECT (gsearch->find_button), "size_allocate",
	                  G_CALLBACK (size_allocate_cb), (gpointer) gsearch->available_options_add_button);
	g_signal_connect (G_OBJECT (gsearch->stop_button), "clicked",
	                  G_CALLBACK (click_stop_cb), (gpointer) gsearch);

	if (gsearch->is_window_accessible) {
		add_atk_namedesc (CTK_WIDGET (gsearch->find_button), NULL, _("Click to perform a search."));
		add_atk_namedesc (CTK_WIDGET (gsearch->stop_button), NULL, _("Click to stop a search."));
	}

	ctk_widget_show_all (container);
	ctk_widget_hide (gsearch->available_options_vbox);
	ctk_widget_hide (gsearch->progress_spinner);
	ctk_widget_hide (gsearch->stop_button);

	ctk_window_set_focus (CTK_WINDOW (gsearch->window),
		CTK_WIDGET (gsearch_history_entry_get_entry (GSEARCH_HISTORY_ENTRY (gsearch->name_contains_entry))));

	ctk_window_set_default (CTK_WINDOW (gsearch->window), gsearch->find_button);

	return gsearch->window;
}

static void
gsearch_window_finalize (GObject * object)
{
        parent_class->finalize (object);
}

static void
gsearch_window_class_init (GSearchWindowClass * klass, void * dara)
{
	GObjectClass * object_class = (GObjectClass *) klass;

	object_class->finalize = gsearch_window_finalize;
	parent_class = g_type_class_peek_parent (klass);
}

GType
gsearch_window_get_type (void)
{
	static GType object_type = 0;

	if (!object_type) {
		static const GTypeInfo object_info = {
			sizeof (GSearchWindowClass),
			NULL,
			NULL,
			(GClassInitFunc) gsearch_window_class_init,
			NULL,
			NULL,
			sizeof (GSearchWindow),
			0,
			(GInstanceInitFunc)(void (*)(void)) gsearch_app_create
		};
		object_type = g_type_register_static (CTK_TYPE_WINDOW, "GSearchWindow", &object_info, 0);
	}
	return object_type;
}

static void
gsearchtool_setup_gsettings_notifications (GSearchWindow * gsearch)

{
	gchar * click_to_activate_pref;

	/* Use the default double click behavior if baul isn't installed */
	if (gsearch->baul_schema_exists == FALSE) {
		gsearch->is_search_results_single_click_to_activate = FALSE;
		return;
	}

	g_signal_connect (gsearch->baul_settings,
	                  "changed::click-policy",
	                  G_CALLBACK (single_click_to_activate_key_changed_cb),
	                  gsearch);

	/* Get value of baul click behavior (single or double click to activate items) */
	click_to_activate_pref = g_settings_get_string (gsearch->baul_settings, "click-policy");

	gsearch->is_search_results_single_click_to_activate =
		(strncmp (click_to_activate_pref, "single", 6) == 0) ? TRUE : FALSE;

	g_free (click_to_activate_pref);
}

int
main (int argc,
      char * argv[])
{
	GSearchWindow * gsearch;
	GOptionContext * context;
	CtkWidget * window;
	GError * error = NULL;
	EggSMClient * client;

	setlocale (LC_ALL, "");
	bindtextdomain (GETTEXT_PACKAGE, CAFELOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	context = g_option_context_new (N_("- the CAFE Search Tool"));
	g_option_context_set_translation_domain(context, GETTEXT_PACKAGE);
	gsearch_setup_goption_descriptions ();
	g_option_context_add_main_entries (context, GSearchGOptionEntries, GETTEXT_PACKAGE);
	g_option_context_add_group (context, ctk_get_option_group (TRUE));
	g_option_context_add_group (context, egg_sm_client_get_option_group ());
	g_option_context_parse (context, &argc, &argv, &error);

	if (error) {
		g_printerr (_("Failed to parse command line arguments: %s\n"), error->message);
		return (-1);
	}

	g_option_context_free (context);

	g_set_application_name (_("Search for Files"));
	ctk_window_set_default_icon_name (CAFE_SEARCH_TOOL_ICON);

	g_object_set (ctk_settings_get_default (), "ctk-button-images", TRUE, NULL);

	gsearchtool_init_stock_icons ();

	window = g_object_new (GSEARCH_TYPE_WINDOW, NULL);
	gsearch = GSEARCH_WINDOW (window);

	ctk_window_set_resizable (CTK_WINDOW (gsearch->window), TRUE);

	g_signal_connect (G_OBJECT (gsearch->window), "delete_event",
	                            G_CALLBACK (quit_cb),
	                            (gpointer) gsearch);
	g_signal_connect (G_OBJECT (gsearch->window), "key_press_event",
	                            G_CALLBACK (key_press_cb),
	                            (gpointer) gsearch);
	g_signal_connect (G_OBJECT (gsearch->window), "window_state_event",
	                            G_CALLBACK (window_state_event_cb),
	                            (gpointer) gsearch);

	if ((client = egg_sm_client_get ()) != NULL) {
		g_signal_connect (client, "save_state",
		                  G_CALLBACK (save_session_cb),
		                  (gpointer) gsearch);
		g_signal_connect (client, "quit",
		                  G_CALLBACK (quit_session_cb),
		                  (gpointer) gsearch);
	}

	ctk_widget_show (gsearch->window);

	gsearchtool_setup_gsettings_notifications (gsearch);

	if (handle_goption_args (gsearch) == FALSE) {
		handle_gsettings_settings (gsearch);
	}

	ctk_main ();
	return 0;
}
