/*
 * UnrealIRCd, src/s_dispatch.c
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

#ifdef _WIN32
#include <WinSock2.h>
#endif

#include "struct.h"
#include "common.h"
#include "sys.h"
#include "res.h"
#include "numeric.h"
#include "version.h"
#ifndef _WIN32
#include <sys/socket.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/resource.h>
#else
#include <io.h>
#endif
#if defined(_SOLARIS)
#include <sys/filio.h>
#endif
#include "inet.h"
#include <stdio.h>
#include <signal.h>
#include <fcntl.h>

#include "sock.h"               /* If FD_ZERO isn't define up to this point,  */
#include <string.h>
#include "proto.h"
                        /* define it (BSD4.2 needs this) */
#include "h.h"
#ifndef NO_FDLIST
#include  "fdlist.h"
#endif

/***************************************************************************************
 * Backend-independent functions.  fd_setselect() and friends                          *
 ***************************************************************************************/
void fd_setselect(int fd, int flags, IOCallbackFunc iocb, void *data)
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
	fde->data = data;

	if (flags & FD_SELECT_READ)
	{
		fde->read_callback = iocb;

		if (flags & FD_SELECT_ONESHOT)
			fde->read_oneshot = 1;
	}
	if (flags & FD_SELECT_WRITE)
	{
		fde->write_callback = iocb;

		if (flags & FD_SELECT_ONESHOT)
			fde->write_oneshot = 1;
	}

	fd_refresh(fd);
}

#ifdef USE_SELECT
# error select() is not supported by the new code yet
#endif

/***************************************************************************************
 * epoll() backend.                                                                    *
 ***************************************************************************************/
#ifdef HAVE_EPOLL

#include <sys/epoll.h>

static int epoll_fd = -1;
static struct epoll_event epfds[MAXCONNECTIONS + 1];

void fd_refresh(int fd)
{
	struct epoll_event ep_event;
	FDEntry *fde = &fd_table[fd];
	unsigned int pflags = 0;
	unsigned int i;
	int op = -1;

	if (epoll_fd == -1)
		epoll_fd = epoll_create(MAXCONNECTIONS);

	if (fde->read_callback)
		pflags |= EPOLLIN;

	if (fde->write_callback)
		pflags |= EPOLLOUT;

	if (fde->read_oneshot || fde->write_oneshot)
		pflags |= EPOLLONESHOT;

	if (pflags == 0 && fde->backend_flags == 0)
		return;
	else if (pflags == 0)
		op = EPOLL_CTL_DEL;
	else if (fde->backend_flags == 0 && pflags != 0)
		op = EPOLL_CTL_ADD;
	else if (fde->backend_flags != pflags)
		op = EPOLL_CTL_MOD;

	if (op == -1)
		return;

	ep_event.events = pflags;
	ep_event.data.ptr = fde;

	if (epoll_ctl(epoll_fd, op, fd, &ep_event) != 0)
	{
		if (ERRNO == P_EWOULDBLOCK || ERRNO == P_EAGAIN)
			return;

		ircd_log(LOG_ERROR, "[BUG?] epoll returned %d", errno);
		return;
	}

	fde->backend_flags = pflags;
}

void fd_select(time_t delay)
{
	int num, p, revents, fd;
	struct epoll_event *epfd;

	if (epoll_fd == -1)
		epoll_fd = epoll_create(MAXCONNECTIONS);

	num = epoll_wait(epoll_fd, epfds, MAXCONNECTIONS, delay);
	if (num <= 0)
		return;

	for (p = 0; p < num; p++)
	{
		FDEntry *fde;
		IOCallbackFunc iocb;
		int evflags = 0;

		epfd = &epfds[p];

		revents = epfd->events;
		if (revents == 0)
			continue;

		fde = epfd->data.ptr;
		fd = fde->fd;

		if (revents & (EPOLLIN | EPOLLHUP | EPOLLERR))
			evflags |= FD_SELECT_READ;

		if (revents & (EPOLLOUT | EPOLLHUP | EPOLLERR))
			evflags |= FD_SELECT_WRITE;

		if (evflags & FD_SELECT_READ)
		{
			iocb = fde->read_callback;
			if (fde->read_oneshot)
				fde->read_callback = NULL;

			if (iocb != NULL)
				iocb(fd, evflags, fde->data);

			fde->read_oneshot = 0;
		}

		if (evflags & FD_SELECT_WRITE)
		{
			iocb = fde->write_callback;
			if (fde->write_oneshot)
				fde->write_callback = NULL;

			if (iocb != NULL)
				iocb(fd, evflags, fde->data);

			fde->write_oneshot = 0;
		}
	}
}

#endif

/***************************************************************************************
 * Poll() backend.                                                                     *
 ***************************************************************************************/
#ifdef USE_POLL

#ifndef _WIN32
# include <sys/poll.h>
# include <poll.h>
#else
#  define poll WSAPoll
#  define POLLRDHUP POLLHUP
#endif

#ifndef POLLRDNORM
# define POLLRDNORM POLLIN
#endif
#ifndef POLLWRNORM
# define POLLWRNORM POLLOUT
#endif

static struct pollfd pollfds[FD_SETSIZE];
static nfds_t nfds = 0;

void fd_refresh(int fd)
{
	FDEntry *fde = &fd_table[fd];
	unsigned int pflags = 0;
	unsigned int i;

	if (fde->read_callback)
		pflags |= (POLLRDNORM | POLLIN);

	if (fde->write_callback)
		pflags |= (POLLWRNORM | POLLOUT);

	pollfds[fde->fd].events = pflags;
	pollfds[fde->fd].fd = pflags ? fde->fd : -1;

	/* tighten maximum pollfd */
	if (pflags && nfds < fde->fd)
		nfds = fde->fd;

	while (nfds > 0 && pollfds[nfds].fd == -1)
		nfds--;
}

void fd_select(time_t delay)
{
	int num, p, revents, fd;
	struct pollfd *pfd;

	num = poll(pollfds, nfds + 1, delay);
	if (num <= 0)
		return;

	for (p = 0; p < (nfds + 1); p++)
	{
		FDEntry *fde;
		IOCallbackFunc iocb;
		int evflags = 0;

		pfd = &pollfds[p];

		revents = pfd->revents;
		fd = pfd->fd;
		if (revents == 0 || fd == -1)
			continue;

		fde = &fd_table[fd];

		if (revents & (POLLRDNORM | POLLIN | POLLHUP | POLLERR))
			evflags |= FD_SELECT_READ;

		if (revents & (POLLWRNORM | POLLOUT | POLLHUP | POLLERR))
			evflags |= FD_SELECT_WRITE;

		if (evflags & FD_SELECT_READ)
		{
			iocb = fde->read_callback;
			if (fde->read_oneshot)
				fde->read_callback = NULL;

			if (iocb != NULL)
				iocb(fd, evflags, fde->data);

			fde->read_oneshot = 0;
		}

		if (evflags & FD_SELECT_WRITE)
		{
			iocb = fde->write_callback;
			if (fde->write_oneshot)
				fde->write_callback = NULL;

			if (iocb != NULL)
				iocb(fd, evflags, fde->data);

			fde->write_oneshot = 0;
		}
	}
}

#endif
