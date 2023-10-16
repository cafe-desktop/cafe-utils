/* screenshot-utils.h - common functions for CAFE Screenshot
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

#ifndef __SCREENSHOT_UTILS_H__
#define __SCREENSHOT_UTILS_H__

#include <ctk/ctk.h>
#include <cdk/cdkx.h>

G_BEGIN_DECLS

typedef void (* SelectAreaCallback) (CdkRectangle *rectangle);

gboolean   screenshot_grab_lock           (void);
void       screenshot_release_lock        (void);
CdkWindow *screenshot_find_current_window (void);
void       screenshot_select_area_async   (SelectAreaCallback callback);
GdkPixbuf *screenshot_get_pixbuf          (CdkWindow *win,
                                           CdkRectangle *rectangle,
                                           gboolean include_pointer,
                                           gboolean include_border,
                                           gboolean include_mask);

void       screenshot_show_error_dialog   (CtkWindow   *parent,
                                           const gchar *message,
                                           const gchar *detail);
void       screenshot_show_gerror_dialog  (CtkWindow   *parent,
                                           const gchar *message,
                                           GError      *error);

G_END_DECLS

#endif /* __SCREENSHOT_UTILS_H__ */
