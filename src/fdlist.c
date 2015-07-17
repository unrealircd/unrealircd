/*
 * UnrealIRCd, src/fdlist.c
 * Copyright (c) 2012 William Pitcock <nenolod@dereferenced.org>
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
#include <sys/stat.h>
#ifdef  UNISTDH
#include <unistd.h>
#endif
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#ifdef _WIN32
#include <io.h>
#endif
#include <fcntl.h>

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
#ifdef DEBUGMODE
		abort();
#endif
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

#ifndef _WIN32
# define OPEN_MODES	S_IRUSR|S_IWUSR
#else
# define OPEN_MODES	S_IREAD|S_IWRITE
#endif

int fd_fileopen(const char *path, unsigned int flags)
{
	FDEntry *fde;
	int fd;
	char comment[FD_DESC_SZ];
	char pathbuf[BUFSIZE];

	fd = open(path, flags, OPEN_MODES);
	if (fd < 0)
		return -1;

	strlcpy(pathbuf, path, sizeof pathbuf);

	snprintf(comment, sizeof comment, "File: %s", unreal_getfilename(pathbuf));

	return fd_open(fd, comment);
}

int fd_unmap(int fd)
{
	FDEntry *fde;
	unsigned int befl;

	if ((fd < 0) || (fd >= MAXCONNECTIONS))
	{
		sendto_realops("[BUG] trying to close fd #%d in fd table, but MAXCONNECTIONS is %d",
				fd, MAXCONNECTIONS);
		ircd_log(LOG_ERROR, "[BUG] trying to close fd #%d in fd table, but MAXCONNECTIONS is %d",
				fd, MAXCONNECTIONS);
#ifdef DEBUGMODE
		abort();
#endif
		return 0;
	}

	fde = &fd_table[fd];
	if (!fde->is_open)
	{
		sendto_realops("[BUG] trying to close fd #%d in fd table, but this FD isn't reported open",
				fd);
		ircd_log(LOG_ERROR, "[BUG] trying to close fd #%d in fd table, but this FD isn't reported open",
				fd);
#ifdef DEBUGMODE
		abort();
#endif
		return 0;
	}

	befl = fde->backend_flags;
	memset(fde, 0, sizeof(FDEntry));

	fde->fd = fd;

	/* only notify the backend if it is actively tracking the FD */
	if (befl)
		fd_refresh(fd);
	
	return 1;
}

void fd_close(int fd)
{
	FDEntry *fde;
	unsigned int befl;

	if (!fd_unmap(fd))
		return;

	CLOSE_SOCK(fd);
}

/* Deregister I/O notification for this file descriptor */
void fd_unnotify(int fd)
{
FDEntry *fde;
#ifdef DEBUGMODE
	ircd_log(LOG_ERROR, "fd_unnotify(): fd=%d", fd);
#endif
	if ((fd < 0) || (fd >= MAXCONNECTIONS))
		return;
	
	fde = &fd_table[fd];
	if (!fde || !fde->is_open)
		return;
		
	fde->read_callback = fde->write_callback = NULL;
	fde->read_oneshot = fde->write_oneshot = 0;
	fd_refresh(fd);
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
#ifdef DEBUGMODE
		abort();
#endif
		return;
	}

	fde = &fd_table[fd];
	if (!fde->is_open)
	{
		sendto_realops("[BUG] trying to modify fd #%d in fd table, but this FD isn't reported open",
				fd);
		ircd_log(LOG_ERROR, "[BUG] trying to modify fd #%d in fd table, but this FD isn't reported open",
				fd);
#ifdef DEBUGMODE
		abort();
#endif
		return;
	}

	strlcpy(fde->desc, desc, FD_DESC_SZ);
}

