/* cafe-screenshot.c - Take a screenshot of the desktop
 *
 * Copyright (C) 2001 Jonathan Blandford <jrb@alum.mit.edu>
 * Copyright (C) 2006 Emmanuele Bassi <ebassi@gnome.org>
 * Copyright (C) 2008 Cosimo Cecchi <cosimoc@gnome.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301
 * USA
 */

/* THERE ARE NO FEATURE REQUESTS ALLOWED */
/* IF YOU WANT YOUR OWN FEATURE -- WRITE THE DAMN THING YOURSELF (-: */
/* MAYBE I LIED... -jrb */

#include <config.h>
#include <cdk/cdkx.h>
#include <cdk/cdkkeysyms.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <locale.h>
#include <glib/gi18n.h>
#include <gio/gio.h>
#include <pwd.h>
#include <X11/Xutil.h>
#include <kanberra-ctk.h>

#include "screenshot-shadow.h"
#include "screenshot-utils.h"
#include "screenshot-save.h"
#include "screenshot-dialog.h"
#include "screenshot-xfer.h"

#define SCREENSHOOTER_ICON "applets-screenshooter"

#define CAFE_SCREENSHOT_SCHEMA "org.cafe.screenshot"
#define INCLUDE_BORDER_KEY      "include-border"
#define INCLUDE_POINTER_KEY     "include-pointer"
#define LAST_SAVE_DIRECTORY_KEY "last-save-directory"
#define BORDER_EFFECT_KEY       "border-effect"
#define DELAY_KEY               "delay"
#define BAUL_PREFERENCES_SCHEMA "org.cafe.baul.preferences"

enum
{
  COLUMN_NICK,
  COLUMN_LABEL,
  COLUMN_ID,

  N_COLUMNS
};

typedef enum {
  SCREENSHOT_EFFECT_NONE,
  SCREENSHOT_EFFECT_SHADOW,
  SCREENSHOT_EFFECT_BORDER
} ScreenshotEffectType;

typedef enum
{
  TEST_LAST_DIR = 0,
  TEST_DESKTOP = 1,
  TEST_TMP = 2,
} TestType;

typedef struct
{
  char *base_uris[3];
  char *retval;
  int iteration;
  TestType type;
  CdkWindow *window;
  CdkRectangle *rectangle;
} AsyncExistenceJob;

static GdkPixbuf *screenshot = NULL;

/* Global variables*/
static char *last_save_dir = NULL;
static char *temporary_file = NULL;
static gboolean save_immediately = FALSE;
static GSettings *settings = NULL;
static gboolean interactive_arg = FALSE;
static guint delay_arg = 0;

/* Options */
static gboolean noninteractive_clipboard_arg = FALSE;
static gboolean take_window_shot = FALSE;
static gboolean take_area_shot = FALSE;
static gboolean include_border = FALSE;
static gboolean include_pointer = TRUE;
static char *border_effect = NULL;
static guint delay = 0;

/* some local prototypes */
static void  display_help           (CtkWindow *parent);
static void  save_done_notification (gpointer   data);
static char *get_desktop_dir        (void);
static void  save_options           (void);

static CtkWidget *border_check = NULL;
static CtkWidget *effect_combo = NULL;
static CtkWidget *effect_label = NULL;
static CtkWidget *effects_vbox = NULL;
static CtkWidget *delay_hbox = NULL;

void loop_dialog_screenshot ();

static void
display_help (CtkWindow *parent)
{
  GError *error = NULL;

  ctk_show_uri_on_window (parent,
                          "help:cafe-user-guide/goseditmainmenu-53",
                          ctk_get_current_event_time (),
                          &error);

  if (error)
    {
      screenshot_show_gerror_dialog (parent,
                                     _("Error loading the help page"),
                                     error);
      g_error_free (error);
    }
}

static void
interactive_dialog_response_cb (CtkDialog *dialog,
                                gint       response,
                                gpointer   user_data)
{
  switch (response)
    {
    case CTK_RESPONSE_HELP:
      g_signal_stop_emission_by_name (dialog, "response");
      display_help (CTK_WINDOW (dialog));
      break;
    default:
      ctk_widget_hide (CTK_WIDGET (dialog));
      break;
    }
}

#define TARGET_TOGGLE_DESKTOP 0
#define TARGET_TOGGLE_WINDOW  1
#define TARGET_TOGGLE_AREA    2

static void
target_toggled_cb (CtkToggleButton *button,
                   gpointer         data)
{
  int target_toggle = GPOINTER_TO_INT (data);

  if (ctk_toggle_button_get_active (button))
    {
      take_window_shot = (target_toggle == TARGET_TOGGLE_WINDOW);
      take_area_shot = (target_toggle == TARGET_TOGGLE_AREA);

      ctk_widget_set_sensitive (border_check, take_window_shot);
      ctk_widget_set_sensitive (effect_combo, take_window_shot);
      ctk_widget_set_sensitive (effect_label, take_window_shot);

      ctk_widget_set_sensitive (delay_hbox, !take_area_shot);
      ctk_widget_set_sensitive (effects_vbox, !take_area_shot);
    }
}

static void
delay_spin_value_changed_cb (CtkSpinButton *button)
{
  delay = ctk_spin_button_get_value_as_int (button);
}

static void
include_border_toggled_cb (CtkToggleButton *button,
                           gpointer         data)
{
  include_border = ctk_toggle_button_get_active (button);
}

static void
include_pointer_toggled_cb (CtkToggleButton *button,
                            gpointer         data)
{
  include_pointer = ctk_toggle_button_get_active (button);
}

static void
effect_combo_changed_cb (CtkComboBox *combo,
                         gpointer     user_data)
{
  CtkTreeIter iter;

  if (ctk_combo_box_get_active_iter (combo, &iter))
    {
      CtkTreeModel *model;
      gchar *effect;

      model = ctk_combo_box_get_model (combo);
      ctk_tree_model_get (model, &iter, COLUMN_NICK, &effect, -1);

      g_assert (effect != NULL);

      g_free (border_effect);
      border_effect = effect; /* gets free'd later */
    }
}

static gint
key_press_cb (CtkWidget* widget, CdkEventKey* event, gpointer data)
{
  if (event->keyval == CDK_KEY_F1)
    {
      display_help (CTK_WINDOW (widget));
      return TRUE;
    }

  return FALSE;
}

typedef struct {
  ScreenshotEffectType id;
  const gchar *label;
  const gchar *nick;
} ScreenshotEffect;

/* Translators:
 * these are the names of the effects available which will be
 * displayed inside a combo box in interactive mode for the user
 * to chooser.
 */
static const ScreenshotEffect effects[] = {
  { SCREENSHOT_EFFECT_NONE, N_("None"), "none" },
  { SCREENSHOT_EFFECT_SHADOW, N_("Drop shadow"), "shadow" },
  { SCREENSHOT_EFFECT_BORDER, N_("Border"), "border" }
};

static guint n_effects = G_N_ELEMENTS (effects);

static CtkWidget *
create_effects_combo (void)
{
  CtkWidget *retval;
  CtkListStore *model;
  CtkCellRenderer *renderer;
  gint i;

  model = ctk_list_store_new (N_COLUMNS,
                              G_TYPE_STRING,
                              G_TYPE_STRING,
                              G_TYPE_UINT);

  for (i = 0; i < n_effects; i++)
    {
      CtkTreeIter iter;

      ctk_list_store_insert (model, &iter, i);
      ctk_list_store_set (model, &iter,
                          COLUMN_ID, effects[i].id,
                          COLUMN_LABEL, gettext (effects[i].label),
                          COLUMN_NICK, effects[i].nick,
                          -1);
    }

  retval = ctk_combo_box_new ();
  ctk_combo_box_set_model (CTK_COMBO_BOX (retval),
                           CTK_TREE_MODEL (model));
  g_object_unref (model);

  switch (border_effect[0])
    {
    case 's': /* shadow */
      ctk_combo_box_set_active (CTK_COMBO_BOX (retval),
                                SCREENSHOT_EFFECT_SHADOW);
      break;
    case 'b': /* border */
      ctk_combo_box_set_active (CTK_COMBO_BOX (retval),
                                SCREENSHOT_EFFECT_BORDER);
      break;
    case 'n': /* none */
      ctk_combo_box_set_active (CTK_COMBO_BOX (retval),
                                SCREENSHOT_EFFECT_NONE);
      break;
    default:
      break;
    }

  renderer = ctk_cell_renderer_text_new ();
  ctk_cell_layout_pack_start (CTK_CELL_LAYOUT (retval), renderer, TRUE);
  ctk_cell_layout_set_attributes (CTK_CELL_LAYOUT (retval), renderer,
                                  "text", COLUMN_LABEL,
                                  NULL);

  g_signal_connect (retval, "changed",
                    G_CALLBACK (effect_combo_changed_cb),
                    NULL);

  return retval;
}

static void
create_effects_frame (CtkWidget   *outer_vbox,
                      const gchar *frame_title)
{
  CtkWidget *main_vbox, *vbox, *hbox;
  CtkWidget *label;
  CtkWidget *check;
  CtkWidget *combo;
  gchar *title;

  main_vbox = ctk_box_new (CTK_ORIENTATION_VERTICAL, 6);
  ctk_widget_set_sensitive (main_vbox, !take_area_shot);
  ctk_box_pack_start (CTK_BOX (outer_vbox), main_vbox, FALSE, FALSE, 0);
  ctk_widget_show (main_vbox);
  effects_vbox = main_vbox;

  title = g_strconcat ("<b>", frame_title, "</b>", NULL);
  label = ctk_label_new (title);
  ctk_label_set_use_markup (CTK_LABEL (label), TRUE);
  ctk_label_set_xalign (CTK_LABEL (label), 0.0);
  ctk_label_set_yalign (CTK_LABEL (label), 0.5);
  ctk_box_pack_start (CTK_BOX (main_vbox), label, FALSE, FALSE, 0);
  ctk_widget_show (label);
  g_free (title);

  hbox = ctk_box_new (CTK_ORIENTATION_HORIZONTAL, 12);
  ctk_box_pack_start (CTK_BOX (main_vbox), hbox, FALSE, FALSE, 0);
  ctk_widget_show (hbox);

  vbox = ctk_box_new (CTK_ORIENTATION_VERTICAL, 6);
  ctk_box_pack_start (CTK_BOX (hbox), vbox, FALSE, FALSE, 0);
  ctk_widget_set_margin_start (vbox, 12);
  ctk_widget_show (vbox);

  /** Include pointer **/
  check = ctk_check_button_new_with_mnemonic (_("Include _pointer"));
  ctk_toggle_button_set_active (CTK_TOGGLE_BUTTON (check), include_pointer);
  g_signal_connect (check, "toggled",
                    G_CALLBACK (include_pointer_toggled_cb),
                    NULL);
  ctk_box_pack_start (CTK_BOX (vbox), check, FALSE, FALSE, 0);
  ctk_widget_show (check);

  /** Include window border **/
  check = ctk_check_button_new_with_mnemonic (_("Include the window _border"));
  ctk_widget_set_sensitive (check, take_window_shot);
  ctk_toggle_button_set_active (CTK_TOGGLE_BUTTON (check), include_border);
  g_signal_connect (check, "toggled",
                    G_CALLBACK (include_border_toggled_cb),
                    NULL);
  ctk_box_pack_start (CTK_BOX (vbox), check, FALSE, FALSE, 0);
  ctk_widget_show (check);
  border_check = check;

  /** Effects **/
  hbox = ctk_box_new (CTK_ORIENTATION_HORIZONTAL, 12);
  ctk_box_pack_start (CTK_BOX (vbox), hbox, FALSE, FALSE, 0);
  ctk_widget_show (hbox);

  label = ctk_label_new_with_mnemonic (_("Apply _effect:"));
  ctk_widget_set_sensitive (label, take_window_shot);
  ctk_label_set_xalign (CTK_LABEL (label), 0.0);
  ctk_label_set_yalign (CTK_LABEL (label), 0.5);
  ctk_box_pack_start (CTK_BOX (hbox), label, FALSE, FALSE, 0);
  ctk_widget_show (label);
  effect_label = label;

  combo = create_effects_combo ();
  ctk_widget_set_sensitive (combo, take_window_shot);
  ctk_box_pack_start (CTK_BOX (hbox), combo, FALSE, FALSE, 0);
  ctk_label_set_mnemonic_widget (CTK_LABEL (label), combo);
  ctk_widget_show (combo);
  effect_combo = combo;
}

static void
create_screenshot_frame (CtkWidget   *outer_vbox,
                         const gchar *frame_title)
{
  CtkWidget *main_vbox, *vbox, *hbox;
  CtkWidget *radio;
  CtkWidget *image;
  CtkWidget *spin;
  CtkWidget *label;
  CtkAdjustment *adjust;
  GSList *group;
  gchar *title;

  main_vbox = ctk_box_new (CTK_ORIENTATION_VERTICAL, 6);
  ctk_box_pack_start (CTK_BOX (outer_vbox), main_vbox, FALSE, FALSE, 0);
  ctk_widget_show (main_vbox);

  title = g_strconcat ("<b>", frame_title, "</b>", NULL);
  label = ctk_label_new (title);
  ctk_label_set_use_markup (CTK_LABEL (label), TRUE);
  ctk_label_set_xalign (CTK_LABEL (label), 0.0);
  ctk_label_set_yalign (CTK_LABEL (label), 0.5);
  ctk_box_pack_start (CTK_BOX (main_vbox), label, FALSE, FALSE, 0);
  ctk_widget_show (label);
  g_free (title);

  hbox = ctk_box_new (CTK_ORIENTATION_HORIZONTAL, 12);
  ctk_box_pack_start (CTK_BOX (main_vbox), hbox, FALSE, FALSE, 0);
  ctk_widget_show (hbox);

  image = ctk_image_new_from_icon_name (SCREENSHOOTER_ICON,
                                    CTK_ICON_SIZE_DIALOG);

  ctk_box_pack_start (CTK_BOX (hbox), image, FALSE, FALSE, 0);
  ctk_widget_set_valign (image, CTK_ALIGN_START);
  ctk_widget_show (image);

  vbox = ctk_box_new (CTK_ORIENTATION_VERTICAL, 6);
  ctk_box_pack_start (CTK_BOX (hbox), vbox, FALSE, FALSE, 0);
  ctk_widget_show (vbox);

  /** Grab whole desktop **/
  group = NULL;
  radio = ctk_radio_button_new_with_mnemonic (group,
                                              _("Grab the whole _desktop"));
  if (take_window_shot || take_area_shot)
    ctk_toggle_button_set_active (CTK_TOGGLE_BUTTON (radio), FALSE);
  g_signal_connect (radio, "toggled",
                    G_CALLBACK (target_toggled_cb),
                    GINT_TO_POINTER (TARGET_TOGGLE_DESKTOP));
  ctk_box_pack_start (CTK_BOX (vbox), radio, FALSE, FALSE, 0);
  group = ctk_radio_button_get_group (CTK_RADIO_BUTTON (radio));
  ctk_widget_show (radio);

  /** Grab current window **/
  radio = ctk_radio_button_new_with_mnemonic (group,
                                              _("Grab the current _window"));
  if (take_window_shot)
    ctk_toggle_button_set_active (CTK_TOGGLE_BUTTON (radio), TRUE);
  g_signal_connect (radio, "toggled",
                    G_CALLBACK (target_toggled_cb),
                    GINT_TO_POINTER (TARGET_TOGGLE_WINDOW));
  ctk_box_pack_start (CTK_BOX (vbox), radio, FALSE, FALSE, 0);
  group = ctk_radio_button_get_group (CTK_RADIO_BUTTON (radio));
  ctk_widget_show (radio);

  /** Grab area of the desktop **/
  radio = ctk_radio_button_new_with_mnemonic (group,
                                              _("Select _area to grab"));
  if (take_area_shot)
    ctk_toggle_button_set_active (CTK_TOGGLE_BUTTON (radio), TRUE);
  g_signal_connect (radio, "toggled",
                    G_CALLBACK (target_toggled_cb),
                    GINT_TO_POINTER (TARGET_TOGGLE_AREA));
  ctk_box_pack_start (CTK_BOX (vbox), radio, FALSE, FALSE, 0);
  ctk_widget_show (radio);

  /** Grab after delay **/
  delay_hbox = ctk_box_new (CTK_ORIENTATION_HORIZONTAL, 6);
  ctk_widget_set_sensitive (delay_hbox, !take_area_shot);
  ctk_box_pack_start (CTK_BOX (vbox), delay_hbox, FALSE, FALSE, 0);
  ctk_widget_show (delay_hbox);

  /* translators: this is the first part of the "grab after a
   * delay of <spin button> seconds".
   */
  label = ctk_label_new_with_mnemonic (_("Grab _after a delay of"));
  ctk_label_set_xalign (CTK_LABEL (label), 0.0);
  ctk_label_set_yalign (CTK_LABEL (label), 0.5);
  ctk_box_pack_start (CTK_BOX (delay_hbox), label, FALSE, FALSE, 0);
  ctk_widget_show (label);

  adjust = CTK_ADJUSTMENT (ctk_adjustment_new ((gdouble) delay,
                                               0.0, 99.0,
                                               1.0,  1.0,
                                               0.0));
  spin = ctk_spin_button_new (adjust, 1.0, 0);
  g_signal_connect (spin, "value-changed",
                    G_CALLBACK (delay_spin_value_changed_cb),
                    NULL);
  ctk_box_pack_start (CTK_BOX (delay_hbox), spin, FALSE, FALSE, 0);
  ctk_label_set_mnemonic_widget (CTK_LABEL (label), spin);
  ctk_widget_show (spin);

  /* translators: this is the last part of the "grab after a
   * delay of <spin button> seconds".
   */
  label = ctk_label_new (_("seconds"));
  ctk_label_set_xalign (CTK_LABEL (label), 0.0);
  ctk_label_set_yalign (CTK_LABEL (label), 0.5);
  ctk_box_pack_end (CTK_BOX (delay_hbox), label, FALSE, FALSE, 0);
  ctk_widget_show (label);
}

static CtkWidget *
create_interactive_dialog (void)
{
  CtkWidget *retval;
  CtkWidget *main_vbox;
  CtkWidget *content_area;

  retval = ctk_dialog_new ();
  ctk_window_set_resizable (CTK_WINDOW (retval), FALSE);
  ctk_container_set_border_width (CTK_CONTAINER (retval), 5);
  content_area = ctk_dialog_get_content_area (CTK_DIALOG (retval));
  ctk_box_set_spacing (CTK_BOX (content_area), 2);
  ctk_window_set_title (CTK_WINDOW (retval), _("Take Screenshot"));

  /* main container */
  main_vbox = ctk_box_new (CTK_ORIENTATION_VERTICAL, 18);
  ctk_container_set_border_width (CTK_CONTAINER (main_vbox), 5);
  ctk_box_pack_start (CTK_BOX (content_area), main_vbox, TRUE, TRUE, 0);
  ctk_widget_show (main_vbox);

  create_screenshot_frame (main_vbox, _("Take Screenshot"));
  create_effects_frame (main_vbox, _("Effects"));
  ctk_dialog_add_buttons (CTK_DIALOG (retval),
                          CTK_STOCK_HELP, CTK_RESPONSE_HELP,
                          CTK_STOCK_CANCEL, CTK_RESPONSE_CANCEL,
                          _("Take _Screenshot"), CTK_RESPONSE_OK,
                          NULL);
  ctk_dialog_set_default_response (CTK_DIALOG (retval), CTK_RESPONSE_OK);

  /* we need to block on "response" and keep showing the interactive
   * dialog in case the user did choose "help"
   */
  g_signal_connect (retval, "response",
                    G_CALLBACK (interactive_dialog_response_cb),
                    NULL);

  g_signal_connect (G_OBJECT (retval), "key-press-event",
                    G_CALLBACK(key_press_cb),
                    NULL);

  return retval;
}

static void
save_folder_to_settings (ScreenshotDialog *dialog)
{
  char *folder;

  folder = screenshot_dialog_get_folder (dialog);
  g_settings_set_string (settings,
                         LAST_SAVE_DIRECTORY_KEY, folder);

  g_free (folder);
}

static void
set_recent_entry (ScreenshotDialog *dialog)
{
  char *uri, *app_exec = NULL;
  CtkRecentManager *recent;
  CtkRecentData recent_data;
  GAppInfo *app;
  const char *exec_name = NULL;
  static char * groups[2] = { "Graphics", NULL };

  app = g_app_info_get_default_for_type ("image/png", TRUE);

  if (!app) {
    /* return early, as this would be an useless recent entry anyway. */
    return;
  }

  uri = screenshot_dialog_get_uri (dialog);
  recent = ctk_recent_manager_get_default ();

  exec_name = g_app_info_get_executable (app);
  app_exec = g_strjoin (" ", exec_name, "%u", NULL);

  recent_data.display_name = NULL;
  recent_data.description = NULL;
  recent_data.mime_type = "image/png";
  recent_data.app_name = "CAFE Screenshot";
  recent_data.app_exec = app_exec;
  recent_data.groups = groups;
  recent_data.is_private = FALSE;

  ctk_recent_manager_add_full (recent, uri, &recent_data);

  g_object_unref (app);
  g_free (app_exec);
  g_free (uri);
}

static void
error_dialog_response_cb (CtkDialog *d,
                          gint response,
                          ScreenshotDialog *dialog)
{
  ctk_widget_destroy (CTK_WIDGET (d));

  screenshot_dialog_focus_entry (dialog);
}

static void
save_callback (TransferResult result,
               char *error_message,
               gpointer data)
{
  ScreenshotDialog *dialog = data;
  CtkWidget *toplevel;

  toplevel = screenshot_dialog_get_toplevel (dialog);
  screenshot_dialog_set_busy (dialog, FALSE);

  if (result == TRANSFER_OK)
    {
      save_folder_to_settings (dialog);
      set_recent_entry (dialog);
      ctk_widget_destroy (toplevel);

      /* we're done, stop the mainloop now */
      ctk_main_quit ();
    }
  else if (result == TRANSFER_OVERWRITE ||
           result == TRANSFER_CANCELLED)
    {
      /* user has canceled the overwrite dialog or the transfer itself, let him
       * choose another name.
       */
      screenshot_dialog_focus_entry (dialog);
    }
  else /* result == TRANSFER_ERROR */
    {
      /* we had an error, display a dialog to the user and let him choose
       * another name/location to save the screenshot.
       */
      CtkWidget *error_dialog;
      char *uri;

      uri = screenshot_dialog_get_uri (dialog);
      error_dialog = ctk_message_dialog_new (CTK_WINDOW (toplevel),
                                       CTK_DIALOG_DESTROY_WITH_PARENT,
                                       CTK_MESSAGE_ERROR,
                                       CTK_BUTTONS_OK,
                                       _("Error while saving screenshot"));
      /* translators: first %s is the file path, second %s is the VFS error */
      ctk_message_dialog_format_secondary_text (CTK_MESSAGE_DIALOG (error_dialog),
                                                _("Impossible to save the screenshot "
                                                  "to %s.\n Error was %s.\n Please choose another "
                                                  "location and retry."), uri, error_message);
      ctk_widget_show (error_dialog);
      g_signal_connect (error_dialog,
                        "response",
                        G_CALLBACK (error_dialog_response_cb),
                        dialog);

      g_free (uri);
    }

}

static void
try_to_save (ScreenshotDialog *dialog,
	     const char       *target)
{
  GFile *source_file, *target_file;

  g_assert (temporary_file);

  screenshot_dialog_set_busy (dialog, TRUE);

  source_file = g_file_new_for_path (temporary_file);
  target_file = g_file_new_for_uri (target);

  screenshot_xfer_uri (source_file,
                       target_file,
                       screenshot_dialog_get_toplevel (dialog),
                       save_callback, dialog);

  /* screenshot_xfer_uri () holds a ref, so we can unref now */
  g_object_unref (source_file);
  g_object_unref (target_file);
}

static void
save_done_notification (gpointer data)
{
  ScreenshotDialog *dialog = data;

  temporary_file = g_strdup (screenshot_save_get_filename ());
  screenshot_dialog_enable_dnd (dialog);

  if (save_immediately)
    {
      CtkWidget *toplevel;

      toplevel = screenshot_dialog_get_toplevel (dialog);
      ctk_dialog_response (CTK_DIALOG (toplevel), CTK_RESPONSE_OK);
    }
}

static void
save_screenshot_in_clipboard (CdkDisplay *display, GdkPixbuf *screenshot)
{
  CtkClipboard *clipboard =
    ctk_clipboard_get_for_display (display, CDK_SELECTION_CLIPBOARD);
  ctk_clipboard_set_image (clipboard, screenshot);
}

static void
screenshot_dialog_response_cb (CtkDialog *d,
                               gint response_id,
                               ScreenshotDialog *dialog)
{
  char *uri;

  if (response_id == CTK_RESPONSE_HELP)
    {
      display_help (CTK_WINDOW (d));
    }
  else if (response_id == SCREENSHOT_RESPONSE_NEW)
    {
      ctk_widget_destroy (CTK_WIDGET (d));
      ctk_main_quit ();
      interactive_arg = TRUE;
      loop_dialog_screenshot();
    }
  else if (response_id == CTK_RESPONSE_OK)
    {
      uri = screenshot_dialog_get_uri (dialog);
      if (temporary_file == NULL)
        {
          save_immediately = TRUE;
          screenshot_dialog_set_busy (dialog, TRUE);
        }
      else
        {
          /* we've saved the temporary file, lets try to copy it to the
           * correct location.
           */
          try_to_save (dialog, uri);
        }
      g_free (uri);
    }
  else if (response_id == SCREENSHOT_RESPONSE_COPY)
    {
      save_screenshot_in_clipboard (ctk_widget_get_display (CTK_WIDGET (d)),
                                    screenshot_dialog_get_screenshot (dialog));
    }
  else /* dialog was canceled */
    {
      ctk_widget_destroy (CTK_WIDGET (d));
      ctk_main_quit ();
    }
}


static void
run_dialog (ScreenshotDialog *dialog)
{
  CtkWidget *toplevel;

  toplevel = screenshot_dialog_get_toplevel (dialog);

  ctk_widget_show (toplevel);

  g_signal_connect (toplevel,
                    "response",
                    G_CALLBACK (screenshot_dialog_response_cb),
                    dialog);
}

static void
play_sound_effect (CdkWindow *window)
{
  ka_context *c;
  ka_proplist *p = NULL;
  int res;

  c = ka_ctk_context_get ();

  res = ka_proplist_create (&p);
  if (res < 0)
    goto done;

  res = ka_proplist_sets (p, KA_PROP_EVENT_ID, "screen-capture");
  if (res < 0)
    goto done;

  res = ka_proplist_sets (p, KA_PROP_EVENT_DESCRIPTION, _("Screenshot taken"));
  if (res < 0)
    goto done;

  if (window != NULL)
    {
      res = ka_proplist_setf (p,
                              KA_PROP_WINDOW_X11_XID,
                              "%lu",
                              (unsigned long) CDK_WINDOW_XID (window));
      if (res < 0)
        goto done;
    }

  ka_context_play_full (c, 0, p, NULL, NULL);

 done:
  if (p != NULL)
    ka_proplist_destroy (p);

}

static void
finish_prepare_screenshot (char *initial_uri, CdkWindow *window, CdkRectangle *rectangle)
{
  ScreenshotDialog *dialog;
  gboolean include_mask = (!take_window_shot && !take_area_shot);

  /* always disable window border for full-desktop or selected-area screenshots */
  if (!take_window_shot)
    screenshot = screenshot_get_pixbuf (window, rectangle, include_pointer, FALSE, include_mask);
  else
    {
      screenshot = screenshot_get_pixbuf (window, rectangle, include_pointer, include_border, include_mask);

      switch (border_effect[0])
        {
        case 's': /* shadow */
          screenshot_add_shadow (&screenshot);
          break;
        case 'b': /* border */
          screenshot_add_border (&screenshot);
          break;
        case 'n': /* none */
        default:
          break;
        }
    }

  /* release now the lock, it was acquired when we were finding the window */
  screenshot_release_lock ();

  if (screenshot == NULL)
    {
      screenshot_show_error_dialog (NULL,
                                    _("Unable to take a screenshot of the current window"),
                                    NULL);
      exit (1);
    }

  play_sound_effect (window);

  if (noninteractive_clipboard_arg) {
    save_screenshot_in_clipboard (cdk_window_get_display (window), screenshot);
    g_free (initial_uri);
    /* Done here: */
    ctk_main_quit ();
    return;
  }

  dialog = screenshot_dialog_new (screenshot, initial_uri, take_window_shot);
  g_free (initial_uri);

  screenshot_save_start (screenshot, save_done_notification, dialog);

  run_dialog (dialog);
}

static void
async_existence_job_free (AsyncExistenceJob *job)
{
  if (!job)
    return;

  g_free (job->base_uris[1]);
  g_free (job->base_uris[2]);

  if (job->rectangle != NULL)
    g_slice_free (CdkRectangle, job->rectangle);

  g_slice_free (AsyncExistenceJob, job);
}

static gboolean
check_file_done (gpointer user_data)
{
  AsyncExistenceJob *job = user_data;

  finish_prepare_screenshot (job->retval, job->window, job->rectangle);

  async_existence_job_free (job);

  return FALSE;
}

static char *
build_uri (AsyncExistenceJob *job)
{
  char *retval, *file_name;
  char *timestamp;
  GDateTime *d;

  d = g_date_time_new_now_local ();
  timestamp = g_date_time_format (d, "%Y-%m-%d %H-%M-%S");
  g_date_time_unref (d);

  if (job->iteration == 0)
    {
      /* translators: this is the name of the file that gets made up
       * with the screenshot if the entire screen is taken */
      file_name = g_strdup_printf (_("Screenshot at %s.png"), timestamp);
    }
  else
    {
      /* translators: this is the name of the file that gets
       * made up with the screenshot if the entire screen is
       * taken */
      file_name = g_strdup_printf (_("Screenshot at %s - %d.png"), timestamp, job->iteration);
    }

  retval = g_build_filename (job->base_uris[job->type], file_name, NULL);
  g_free (file_name);
  g_free (timestamp);

  return retval;
}

static gboolean
try_check_file (GIOSchedulerJob *io_job,
                GCancellable *cancellable,
                gpointer data)
{
  AsyncExistenceJob *job = data;
  GFile *file;
  GFileInfo *info;
  GError *error;
  char *uri;

retry:
  error = NULL;
  uri = build_uri (job);
  file = g_file_new_for_uri (uri);

  info = g_file_query_info (file, G_FILE_ATTRIBUTE_STANDARD_TYPE,
			    G_FILE_QUERY_INFO_NONE, cancellable, &error);
  if (info != NULL)
    {
      /* file already exists, iterate again */
      g_object_unref (info);
      g_object_unref (file);
      g_free (uri);

      (job->iteration)++;

      goto retry;
    }
  else
    {
      /* see the error to check whether the location is not accessible
       * or the file does not exist.
       */
      if (error->code == G_IO_ERROR_NOT_FOUND)
        {
          GFile *parent;

          /* if the parent directory doesn't exist as well, forget the saved
           * directory and treat this as a generic error.
           */

          parent = g_file_get_parent (file);

          if (!g_file_query_exists (parent, NULL))
            {
              (job->type)++;
              job->iteration = 0;

              g_object_unref (file);
              g_object_unref (parent);
              goto retry;
            }
          else
            {
              job->retval = uri;

              g_object_unref (parent);
              goto out;
            }
        }
      else
        {
          /* another kind of error, assume this location is not
           * accessible.
           */
          g_free (uri);
          if (job->type == TEST_TMP)
            {
              job->retval = NULL;
              goto out;
            }
          else
            {
              (job->type)++;
              job->iteration = 0;

              g_error_free (error);
              g_object_unref (file);
              goto retry;
            }
        }
    }

out:
  g_error_free (error);
  g_object_unref (file);

  g_io_scheduler_job_send_to_mainloop_async (io_job,
                                             check_file_done,
                                             job,
                                             NULL);
  return FALSE;
}

static CdkWindow *
find_current_window (void)
{
  CdkWindow *window;

  if (!screenshot_grab_lock ())
    exit (0);

  if (take_window_shot)
    {
      window = screenshot_find_current_window ();
      if (!window)
	{
	  take_window_shot = FALSE;
	  window = cdk_get_default_root_window ();
	}
    }
  else
    {
      window = cdk_get_default_root_window ();
    }

  return window;
}

static void
push_check_file_job (CdkRectangle *rectangle)
{
  AsyncExistenceJob *job;

  job = g_slice_new0 (AsyncExistenceJob);
  job->base_uris[0] = last_save_dir;
  /* we'll have to free these two */
  job->base_uris[1] = get_desktop_dir ();
  job->base_uris[2] = g_strconcat ("file://", g_get_tmp_dir (), NULL);
  job->iteration = 0;
  job->type = TEST_LAST_DIR;
  job->window = find_current_window ();

  if (rectangle != NULL)
    {
      job->rectangle = g_slice_new0 (CdkRectangle);
      job->rectangle->x = rectangle->x;
      job->rectangle->y = rectangle->y;
      job->rectangle->width = rectangle->width;
      job->rectangle->height = rectangle->height;
    }

  /* Check if the area selection was cancelled */
  if (job->rectangle &&
      (job->rectangle->width == 0 || job->rectangle->height == 0))
    {
      async_existence_job_free (job);
      ctk_main_quit ();
      return;
    }

  g_io_scheduler_push_job (try_check_file,
                           job,
                           NULL,
                           0, NULL);

}

static void
rectangle_found_cb (CdkRectangle *rectangle)
{
  push_check_file_job (rectangle);
}

static void
prepare_screenshot (void)
{
  if (take_area_shot)
    screenshot_select_area_async (rectangle_found_cb);
  else
    push_check_file_job (NULL);
}

static gboolean
prepare_screenshot_timeout (gpointer data)
{
  prepare_screenshot ();
  save_options ();

  return FALSE;
}


static gchar *
get_desktop_dir (void)
{
  gboolean desktop_is_home_dir = FALSE;
  gchar *desktop_dir;

  /* Check if baul schema is installed before trying to read settings */
  GSettingsSchema *schema = g_settings_schema_source_lookup (g_settings_schema_source_get_default (),
                                                             BAUL_PREFERENCES_SCHEMA,
                                                             FALSE);

  if (schema != NULL) {
    GSettings *baul_prefs;

    baul_prefs = g_settings_new (BAUL_PREFERENCES_SCHEMA);
    desktop_is_home_dir = g_settings_get_boolean (baul_prefs, "desktop-is-home-dir");

    g_object_unref (baul_prefs);
    g_settings_schema_unref (schema);
  }

  if (desktop_is_home_dir)
    desktop_dir = g_strconcat ("file://", g_get_home_dir (), NULL);
  else
    desktop_dir = g_strconcat ("file://", g_get_user_special_dir (G_USER_DIRECTORY_DESKTOP), NULL);

  return desktop_dir;
}

/* Taken from cafe-vfs-utils.c */
static char *
expand_initial_tilde (const char *path)
{
  char *slash_after_user_name, *user_name;
  struct passwd *passwd_file_entry;

  if (path[1] == '/' || path[1] == '\0') {
    return g_strconcat (g_get_home_dir (), &path[1], NULL);
  }

  slash_after_user_name = strchr (&path[1], '/');
  if (slash_after_user_name == NULL) {
    user_name = g_strdup (&path[1]);
  } else {
    user_name = g_strndup (&path[1],
                           slash_after_user_name - &path[1]);
  }
  passwd_file_entry = getpwnam (user_name);
  g_free (user_name);

  if (passwd_file_entry == NULL || passwd_file_entry->pw_dir == NULL) {
    return g_strdup (path);
  }

  return g_strconcat (passwd_file_entry->pw_dir,
                      slash_after_user_name,
                      NULL);
}

/* Load options */
static void
load_options (void)
{
  /* Find various dirs */
  last_save_dir = g_settings_get_string (settings,
                                         LAST_SAVE_DIRECTORY_KEY);
  if (!last_save_dir || !last_save_dir[0])
    {
      last_save_dir = get_desktop_dir ();
    }
  else if (last_save_dir[0] == '~')
    {
      char *tmp = expand_initial_tilde (last_save_dir);
      g_free (last_save_dir);
      last_save_dir = tmp;
    }

  include_border = g_settings_get_boolean (settings,
                                           INCLUDE_BORDER_KEY);

  include_pointer = g_settings_get_boolean (settings,
                                            INCLUDE_POINTER_KEY);

  border_effect = g_settings_get_string (settings,
                                         BORDER_EFFECT_KEY);
  if (!border_effect)
    border_effect = g_strdup ("none");

  delay = g_settings_get_int (settings, DELAY_KEY);
}

static void
save_options (void)
{
  g_settings_set_boolean (settings,
                          INCLUDE_BORDER_KEY, include_border);
  g_settings_set_boolean (settings,
                          INCLUDE_POINTER_KEY, include_pointer);
  g_settings_set_int (settings, DELAY_KEY, delay);
  g_settings_set_string (settings,
                         BORDER_EFFECT_KEY, border_effect);
}


static void
register_screenshooter_icon (CtkIconFactory * factory)
{
  CtkIconSource *source;
  CtkIconSet *icon_set;

  source = ctk_icon_source_new ();
  ctk_icon_source_set_icon_name (source, SCREENSHOOTER_ICON);

  icon_set = ctk_icon_set_new ();
  ctk_icon_set_add_source (icon_set, source);

  ctk_icon_factory_add (factory, SCREENSHOOTER_ICON, icon_set);
  ctk_icon_set_unref (icon_set);
  ctk_icon_source_free (source);
}

static void
screenshooter_init_stock_icons (void)
{
  CtkIconFactory *factory;

  factory = ctk_icon_factory_new ();
  ctk_icon_factory_add_default (factory);

  register_screenshooter_icon (factory);
  g_object_unref (factory);
}

void
loop_dialog_screenshot ()
{
  /* interactive mode overrides everything */
  if (interactive_arg)
    {
      CtkWidget *dialog;
      gint response;

      dialog = create_interactive_dialog ();
      response = ctk_dialog_run (CTK_DIALOG (dialog));
      ctk_widget_destroy (dialog);

      switch (response)
        {
        case CTK_RESPONSE_DELETE_EVENT:
        case CTK_RESPONSE_CANCEL:
          return;
        case CTK_RESPONSE_OK:
          break;
        default:
          g_assert_not_reached ();
          break;
        }
    }

  if (((delay > 0 && interactive_arg) || delay_arg > 0) &&
      !take_area_shot)
    {
      g_timeout_add (delay * 1000,
		     prepare_screenshot_timeout,
		     NULL);
    }
  else
    {
      if (interactive_arg)
        {
          /* HACK: give time to the dialog to actually disappear.
           * We don't have any way to tell when the compositor has finished
           * re-drawing.
           */
          g_timeout_add (200,
                         prepare_screenshot_timeout, NULL);
        }
      else
        g_idle_add (prepare_screenshot_timeout, NULL);
    }

  ctk_main ();
}

/* main */
int
main (int argc, char *argv[])
{
  GOptionContext *context;
  gboolean window_arg = FALSE;
  gboolean area_arg = FALSE;
  gboolean include_border_arg = FALSE;
  gboolean disable_border_arg = FALSE;
  gchar *border_effect_arg = NULL;
  gboolean version_arg = FALSE;
  GError *error = NULL;

  const GOptionEntry entries[] = {
    { "window", 'w', 0, G_OPTION_ARG_NONE, &window_arg, N_("Grab a window instead of the entire screen"), NULL },
    { "area", 'a', 0, G_OPTION_ARG_NONE, &area_arg, N_("Grab an area of the screen instead of the entire screen"), NULL },
    { "clipboard", 'c', 0, G_OPTION_ARG_NONE, &noninteractive_clipboard_arg, N_("Send grabbed area directly to the clipboard"), NULL },
    { "include-border", 'b', 0, G_OPTION_ARG_NONE, &include_border_arg, N_("Include the window border with the screenshot"), NULL },
    { "remove-border", 'B', 0, G_OPTION_ARG_NONE, &disable_border_arg, N_("Remove the window border from the screenshot"), NULL },
    { "delay", 'd', 0, G_OPTION_ARG_INT, &delay_arg, N_("Take screenshot after specified delay [in seconds]"), N_("seconds") },
    { "border-effect", 'e', 0, G_OPTION_ARG_STRING, &border_effect_arg, N_("Effect to add to the border (shadow, border or none)"), N_("effect") },
    { "interactive", 'i', 0, G_OPTION_ARG_NONE, &interactive_arg, N_("Interactively set options"), NULL },
    { "version", 0, 0, G_OPTION_ARG_NONE, &version_arg, N_("Print version information and exit"), NULL },
    { NULL },
  };

  setlocale (LC_ALL, "");
  bindtextdomain (GETTEXT_PACKAGE, CAFELOCALEDIR);
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
  textdomain (GETTEXT_PACKAGE);

  context = g_option_context_new (_("Take a picture of the screen"));
  g_option_context_set_ignore_unknown_options (context, FALSE);
  g_option_context_set_help_enabled (context, TRUE);
  g_option_context_add_main_entries (context, entries, GETTEXT_PACKAGE);
  g_option_context_add_group (context, ctk_get_option_group (TRUE));

  g_option_context_parse (context, &argc, &argv, &error);

  if (error) {
    g_critical ("Unable to parse arguments: %s", error->message);
    g_error_free (error);
    g_option_context_free (context);
    exit (1);
  }

  g_option_context_free (context);

  if (version_arg) {
    g_print ("%s %s\n", g_get_application_name (), VERSION);
    exit (EXIT_SUCCESS);
  }

  if (interactive_arg && noninteractive_clipboard_arg) {
    g_printerr (_("Conflicting options: --clipboard and --interactive should not be "
                  "used at the same time.\n"));
    exit (1);
  }

  if (window_arg && area_arg) {
    g_printerr (_("Conflicting options: --window and --area should not be "
                  "used at the same time.\n"));
    exit (1);
  }

  ctk_window_set_default_icon_name (SCREENSHOOTER_ICON);

  g_object_set (ctk_settings_get_default (), "ctk-button-images", TRUE, NULL);

  screenshooter_init_stock_icons ();

  settings = g_settings_new (CAFE_SCREENSHOT_SCHEMA);
  load_options ();
  /* allow the command line to override options */
  if (window_arg)
    take_window_shot = TRUE;

  if (area_arg)
    take_area_shot = TRUE;

  if (include_border_arg)
    include_border = TRUE;

  if (disable_border_arg)
    include_border = FALSE;

  if (border_effect_arg)
    {
      g_free (border_effect);
      border_effect = border_effect_arg;
    }

  if (delay_arg > 0)
    delay = delay_arg;

  loop_dialog_screenshot();

  return EXIT_SUCCESS;
}
