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

/*
 * TS6 UID generator based on Atheme protocol/base36uid.
 *
 * The changes are simple -- numerics are always 3 bytes, and id lengths
 * are always 9 bytes.
 */

static char new_uid[IDLEN];

void uid_init(void)
{
	int i;

	memset(new_uid, 0, sizeof new_uid);

	/* copy the SID */
	memcpy(new_uid, me.id, 3);
	memcpy(new_uid + 3, "AAAAA@", 6);
}

static void uid_increment(size_t i)
{
	if (i != 3)    /* Not reached server SID portion yet? */
	{
		if (new_uid[i] == 'Z')
			new_uid[i] = '0';
		else if (new_uid[i] == '9')
		{
			new_uid[i] = 'A';
			uid_increment(i - 1);
		}
		else
			++new_uid[i];
	}
	else
	{
		if (new_uid[i] == 'Z')
			memcpy(new_uid + 3, "AAAAA@", 6);
		else
			++new_uid[i];
	}
}

const char *uid_get(void)
{
	/* start at the last filled byte. */
	uid_increment(IDLEN - 2);
	return new_uid;
}
