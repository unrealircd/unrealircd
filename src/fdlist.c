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
#include "proto.h"
#include <string.h>

/* new FD management code, based on mowgli.eventloop from atheme, hammered into Unreal by
 * me, nenolod.
 */
FDEntry fd_table[MAXCONNECTIONS + 1];

int fd_open(int fd, const char *desc)
{
	FDEntry *fde;

	if ((fd < 0) || (fd >= MAXCONNECTIONS))
	{
		sendto_realops("[BUG] trying to add fd #%d to fd table, but MAXCONNECTIONS is %d",
				fd, MAXCONNECTIONS);
		ircd_log(LOG_ERROR, "[BUG] trying to add fd #%d to fd table, but MAXCONNECTIONS is %d",
				fd, MAXCONNECTIONS);
		return -1;
	}

	fde = &fd_table[fd];
	memset(fde, 0, sizeof(FDEntry));

	fde->fd = fd;
	fde->is_open = 1;
	fde->backend_flags = 0;
	strlcpy(fde->desc, desc, FD_DESC_SZ);

	return fde->fd;
}

void fd_close(int fd)
{
	FDEntry *fde;

	if ((fd < 0) || (fd >= MAXCONNECTIONS))
	{
		sendto_realops("[BUG] trying to close fd #%d in fd table, but MAXCONNECTIONS is %d",
				fd, MAXCONNECTIONS);
		ircd_log(LOG_ERROR, "[BUG] trying to close fd #%d in fd table, but MAXCONNECTIONS is %d",
				fd, MAXCONNECTIONS);
		return;
	}

	fde = &fd_table[fd];
	if (!fde->is_open)
	{
		sendto_realops("[BUG] trying to close fd #%d in fd table, but this FD isn't reported open",
				fd, MAXCONNECTIONS);
		ircd_log(LOG_ERROR, "[BUG] trying to close fd #%d in fd table, but this FD isn't reported open",
				fd, MAXCONNECTIONS);
		return;
	}

	memset(fde, 0, sizeof(FDEntry));

	fde->fd = fd;
	fd_refresh(fd);

	CLOSE_SOCK(fd);
}

int fd_socket(int family, int type, int protocol, const char *desc)
{
	int fd;

	fd = socket(family, type, protocol);
	if (fd < 0)
		return -1;

	return fd_open(fd, desc);
}

int fd_accept(int sockfd)
{
	const char buf[] = "Incoming connection";
	int fd;

	fd = accept(sockfd, NULL, NULL);
	if (fd < 0)
		return -1;

	return fd_open(fd, buf);
}

void fd_desc(int fd, const char *desc)
{
	FDEntry *fde;

	if ((fd < 0) || (fd >= MAXCONNECTIONS))
	{
		sendto_realops("[BUG] trying to modify fd #%d in fd table, but MAXCONNECTIONS is %d",
				fd, MAXCONNECTIONS);
		ircd_log(LOG_ERROR, "[BUG] trying to modify fd #%d in fd table, but MAXCONNECTIONS is %d",
				fd, MAXCONNECTIONS);
		return;
	}

	fde = &fd_table[fd];
	if (!fde->is_open)
	{
		sendto_realops("[BUG] trying to modify fd #%d in fd table, but this FD isn't reported open",
				fd, MAXCONNECTIONS);
		ircd_log(LOG_ERROR, "[BUG] trying to modify fd #%d in fd table, but this FD isn't reported open",
				fd, MAXCONNECTIONS);
		return;
	}

	strlcpy(fde->desc, desc, FD_DESC_SZ);
}

