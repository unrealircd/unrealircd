/************************************************************************
 *   Unreal Internet Relay Chat Daemon, src/fdlist.c
 *   Copyright (C) Mika Nystrom
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

/* $Id$ */

#include "struct.h"
#include "common.h"
#include "sys.h"
#include "h.h"
#include "config.h"
#include "fdlist.h"

void addto_fdlist(int fd, fdlist * listp)
{
	int  index;

	if ((index = ++listp->last_entry) >= MAXCONNECTIONS)
	{
		/*
		 * list too big.. must exit 
		 */
		--listp->last_entry;
		ircd_log(LOG_ERROR, "fdlist.c list too big, must exit...");
#ifdef	USE_SYSLOG
		(void)syslog(LOG_CRIT, "fdlist.c list too big.. must exit");
#endif
		abort();
	}
	else
		listp->entry[index] = fd;
	return;
}

void delfrom_fdlist(int fd, fdlist * listp)
{
	int  i;

	for (i = listp->last_entry; i; i--)
	{
		if (listp->entry[i] == fd)
			break;
	}
	if (!i)
		return;		/*
				 * could not find it! 
				 */
	/*
	 * swap with last_entry 
	 */
	if (i == listp->last_entry)
	{
		listp->entry[i] = 0;
		listp->last_entry--;
		return;
	}
	else
	{
		listp->entry[i] = listp->entry[listp->last_entry];
		listp->entry[listp->last_entry] = 0;
		listp->last_entry--;
		return;
	}
}

void init_fdlist(fdlist * listp)
{
	listp->last_entry = 0;
	memset((char *)listp->entry, '\0', sizeof(listp->entry));
	return;
}
