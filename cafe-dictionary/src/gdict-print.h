/* gdict-print.h - print-related helper functions
 *
 * This file is part of CAFE Dictionary
 *
 * Copyright (C) 2005 Emmanuele Bassi
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

#ifndef __GDICT_PRINT_H__
#define __GDICT_PRINT_H__

#include <ctk/ctk.h>
#include <libgdict/gdict-defbox.h>

G_BEGIN_DECLS

void gdict_show_print_preview (CtkWindow   *parent,
                               GdictDefbox *defbox);
void gdict_show_print_dialog  (CtkWindow   *parent,
			       GdictDefbox *defbox);

G_END_DECLS

#endif /* __GDICT_PRINT_H__ */
