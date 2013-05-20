/*
 *   IRC - Internet Relay Chat, src/modules/m_cap.h
 *   (C) 2012 The UnrealIRCd Team
 *
 *   See file AUTHORS in IRC package for additional names of
 *   the programmers.
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 1, or (at your option)
 *   any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef M_CAP_H
#define M_CAP_H

#include "list.h"

typedef struct _clicap {
	struct list_head caplist_node;
	const char *name;
	int cap;
	int flags;
} ClientCapability;

#define CLICAP_FLAGS_NONE               0x0
#define CLICAP_FLAGS_STICKY             0x1
#define CLICAP_FLAGS_CLIACK             0x2

#endif
