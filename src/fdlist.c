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

#include "unrealircd.h"

/* new FD management code, based on mowgli.eventloop from atheme, hammered into Unreal by
 * me, nenolod.
 */
FDEntry fd_table[MAXCONNECTIONS + 1];

/** Notify I/O engine that a file descriptor opened.
 * @param fd	The file descriptor
 * @param desc	Description for in the fd table
 * @param file	Set to 1 if the fd is a file, 0 otherwise (eg: socket)
 * @returns The file descriptor 'fd' or -1 in case of fatal error.
 */
int fd_open(int fd, const char *desc, int file)
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
	fde->is_file = file;
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
	int fd;
	char comment[FD_DESC_SZ];
	char pathbuf[BUFSIZE];

	fd = open(path, flags, OPEN_MODES);
	if (fd < 0)
		return -1;

	strlcpy(pathbuf, path, sizeof pathbuf);

	snprintf(comment, sizeof comment, "File: %s", unreal_getfilename(pathbuf));

	return fd_open(fd, comment, 1);
}

/** Internal function to unmap and optionally close the fd.
 */
int fd_close_ex(int fd, int close_fd)
{
	FDEntry *fde;
	unsigned int befl;
	int is_file = 0;

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
	is_file = fde->is_file;
	memset(fde, 0, sizeof(FDEntry));

	fde->fd = fd;

	/* only notify the backend if it is actively tracking the FD */
	if (befl)
		fd_refresh(fd);

	/* Finally, close the file or socket if requested to do so */
	if (close_fd)
	{
		if (is_file)
			close(fd);
		else
			CLOSE_SOCK(fd);
	}

	return 1;
}

/** Unmap file descriptor.
 * That is: remove it from our list, but don't actually do any
 * close() or closesocket() call.
 * @param fd	The file descriptor
 * @returns 1 on success, 0 on failure
 */
int fd_unmap(int fd)
{
	return fd_close_ex(fd, 0);
}

/** Close file descriptor.
 * That is: remove it from our list AND call close() or closesocket().
 * @param fd	The file descriptor
 */
void fd_close(int fd)
{
	fd_close_ex(fd, 1);
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
	fd_refresh(fd);
}

int fd_socket(int family, int type, int protocol, const char *desc)
{
	int fd;

	fd = socket(family, type, protocol);
	if (fd < 0)
		return -1;

	return fd_open(fd, desc, 0);
}

int fd_accept(int sockfd)
{
	const char buf[] = "Incoming connection";
	int fd;

	fd = accept(sockfd, NULL, NULL);
	if (fd < 0)
		return -1;

	return fd_open(fd, buf, 0);
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

