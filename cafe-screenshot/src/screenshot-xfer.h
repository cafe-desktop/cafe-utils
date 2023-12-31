/* screenshot-xfer.h - file transfer functions for CAFE Screenshot
 *
 * Copyright (C) 2001-2006  Jonathan Blandford <jrb@alum.mit.edu>
 * Copyright (C) 2008  Cosimo Cecchi <cosimoc@gnome.org>
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

#ifndef __SCREENSHOT_XFER_H__
#define __SCREENSHOT_XFER_H__

#include <ctk/ctk.h>
#include <gio/gio.h>

typedef enum
{
  TRANSFER_OK,
  TRANSFER_OVERWRITE,
  TRANSFER_CANCELLED,
  TRANSFER_ERROR
} TransferResult;

typedef void (* TransferCallback) (TransferResult result,
                                   char *error_message,
                                   gpointer data);

void screenshot_xfer_uri (GFile *source_file,
                          GFile *target_file,
                          CtkWidget *parent,
                          TransferCallback done_callback,
                          gpointer done_callback_data);

#endif /* __SCREENSHOT_XFER_H__ */
