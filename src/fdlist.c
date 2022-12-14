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

/* new FD management code, based on mowgli.eventloop from atheme, hammered into Unrealircd by
 * me, nenolod.
 */
FDEntry fd_table[MAXCONNECTIONS + 1];

/** Notify I/O engine that a file descriptor opened.
 * @param fd		The file descriptor
 * @param desc		Description for in the fd table
 * @param close_method	Tell what a subsequent call to fd_close() should do,
 *                      eg close the socket, file or don't close anything.
 * @returns The file descriptor 'fd' or -1 in case of fatal error.
 */
int fd_open(int fd, const char *desc, FDCloseMethod close_method)
{
	FDEntry *fde;

	if ((fd < 0) || (fd >= MAXCONNECTIONS))
	{
		unreal_log(ULOG_ERROR, "io", "BUG_FD_OPEN_OUT_OF_RANGE", NULL,
		           "[BUG] trying to add fd $fd to fd table, but MAXCONNECTIONS is $maxconnections",
		           log_data_integer("fd", fd),
		           log_data_integer("maxconnections", MAXCONNECTIONS));
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
	fde->close_method = close_method;
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

	return fd_open(fd, comment, FDCLOSE_FILE);
}

/** Internal function to unmap and optionally close the fd.
 */
/** Remove file descriptor from our table and possibly close the fd.
 * The fd is closed (or not) according to the method specified in fd_open().
 * @param fd	The file descriptor
 * @returns 1 on success, 0 on failure
 */
int fd_close(int fd)
{
	FDEntry *fde;
	unsigned int befl;
	FDCloseMethod close_method;

	if ((fd < 0) || (fd >= MAXCONNECTIONS))
	{
		unreal_log(ULOG_ERROR, "io", "BUG_FD_CLOSE_OUT_OF_RANGE", NULL,
		           "[BUG] trying to close fd $fd to fd table, but MAXCONNECTIONS is $maxconnections",
		           log_data_integer("fd", fd),
		           log_data_integer("maxconnections", MAXCONNECTIONS));
#ifdef DEBUGMODE
		abort();
#endif
		return 0;
	}

	fde = &fd_table[fd];
	if (!fde->is_open)
	{
		unreal_log(ULOG_ERROR, "io", "BUG_FD_CLOSE_NOT_OPEN", NULL,
		           "[BUG] trying to close fd $fd to fd table, but FD is (already) closed",
		           log_data_integer("fd", fd));
#ifdef DEBUGMODE
		abort();
#endif
		return 0;
	}

	befl = fde->backend_flags;
	close_method = fde->close_method;
	memset(fde, 0, sizeof(FDEntry));

	fde->fd = fd;

	/* only notify the backend if it is actively tracking the FD */
	if (befl)
		fd_refresh(fd);

	/* Finally, close the file or socket if requested to do so */
	switch (close_method)
	{
		case FDCLOSE_SOCKET:
			CLOSE_SOCK(fd);
			break;
		case FDCLOSE_FILE:
			close(fd);
			break;
		case FDCLOSE_NONE:
		default:
			break;
	}

	return 1;
}

/* Deregister I/O notification for this file descriptor */
void fd_unnotify(int fd)
{
	FDEntry *fde;

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

	return fd_open(fd, desc, FDCLOSE_SOCKET);
}

int fd_accept(int sockfd)
{
	const char buf[] = "Incoming connection";
	int fd;

	fd = accept(sockfd, NULL, NULL);
	if (fd < 0)
		return -1;

	return fd_open(fd, buf, FDCLOSE_SOCKET);
}

void fd_desc(int fd, const char *desc)
{
	FDEntry *fde;

	if ((fd < 0) || (fd >= MAXCONNECTIONS))
	{
		unreal_log(ULOG_ERROR, "io", "BUG_FD_DESC_OUT_OF_RANGE", NULL,
		           "[BUG] trying to fd_desc fd $fd in fd table, but MAXCONNECTIONS is $maxconnections",
		           log_data_integer("fd", fd),
		           log_data_integer("maxconnections", MAXCONNECTIONS));
#ifdef DEBUGMODE
		abort();
#endif
		return;
	}

	fde = &fd_table[fd];
	if (!fde->is_open)
	{
		unreal_log(ULOG_ERROR, "io", "BUG_FD_DESC_NOT_OPEN", NULL,
		           "[BUG] trying to fd_desc fd $fd in fd table, but FD is (already) closed",
		           log_data_integer("fd", fd));
#ifdef DEBUGMODE
		abort();
#endif
		return;
	}

	strlcpy(fde->desc, desc, FD_DESC_SZ);
}

