/*
 * UnrealIRCd, src/uid.c
 * Copyright (c) 2013 William Pitcock <nenolod@dereferenced.org>
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

#include "struct.h"
#include "common.h"
#include "sys.h"
#include "h.h"
#include "config.h"
#include "fdlist.h"
#include "proto.h"

void uid_init(void)
{
}

char uid_int_to_char(int v)
{
	if (v < 10)
		return '0'+v;
	else
		return 'A'+v-10;
}

const char *uid_get(void)
{
	aClient *acptr;
	static char uid[IDLEN];
	static int uidcounter = 0;

	uidcounter++;
	if (uidcounter == 36*36)
		uidcounter = 0;

	do
	{
		snprintf(uid, sizeof(uid), "%s%c%c%c%c%c%c",
			me.id,
			uid_int_to_char(getrandom8() % 36),
			uid_int_to_char(getrandom8() % 36),
			uid_int_to_char(getrandom8() % 36),
			uid_int_to_char(getrandom8() % 36),
			uid_int_to_char(uidcounter / 36),
			uid_int_to_char(uidcounter % 36));
		acptr = find_client(uid, NULL);
	} while (acptr);

	return uid;
}
