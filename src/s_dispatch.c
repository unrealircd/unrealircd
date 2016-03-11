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

#include "setup.h"
#include "config.h"

#ifdef BACKEND_POLL
#ifndef _WIN32
# include <poll.h>
#else
# define poll WSAPoll
# define POLLRDHUP POLLHUP
#endif
#endif

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
#include "fdlist.h"

/***************************************************************************************
 * Backend-independent functions.  fd_setselect() and friends                          *
 ***************************************************************************************/
void fd_setselect(int fd, int flags, IOCallbackFunc iocb, void *data)
{
	FDEntry *fde;
#if 0
	ircd_log(LOG_ERROR, "fd_setselect(): fd %d flags %d func %p", fd, flags, &iocb);
#endif
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
	fde->data = data;

	if (flags & FD_SELECT_READ)
	{
		fde->read_callback = iocb;

		if (flags & FD_SELECT_ONESHOT)
			fde->read_oneshot = 1;
		else
			fde->read_oneshot = 0;
	}
	if (flags & FD_SELECT_WRITE)
	{
		fde->write_callback = iocb;

		if (flags & FD_SELECT_ONESHOT)
			fde->write_oneshot = 1;
		else
			fde->write_oneshot = 0;
	} else
	if (flags & FD_SELECT_NOWRITE)
	{
		fde->write_callback = NULL;
		fde->write_oneshot = 0;
	}

	fd_refresh(fd);
}

/***************************************************************************************
 * select() backend.                                                                   *
 ***************************************************************************************/
#ifdef BACKEND_SELECT

#ifndef _WIN32
# include <sys/select.h>
#endif

static int highest_fd = -1;
static fd_set read_fds, write_fds;

void fd_refresh(int fd)
{
	FDEntry *fde = &fd_table[fd];
	unsigned int flags = 0;

	if (fde->read_callback)
	{
		flags |= FD_SELECT_READ;

		FD_SET(fd, &read_fds);
	}
	else
		FD_CLR(fd, &read_fds);

	if (fde->write_callback)
	{
		flags |= FD_SELECT_WRITE;

		FD_SET(fd, &write_fds);
	}
	else
		FD_CLR(fd, &write_fds);

	if (flags && highest_fd < fd)
		highest_fd = fd;

	while (highest_fd > 0 &&
		!(FD_ISSET(highest_fd, &read_fds) || FD_ISSET(highest_fd, &write_fds)))
		highest_fd--;

	fde->backend_flags = flags;
}

void fd_debug(fd_set *f, int highest, char *name)
{
	int i;
	for (i = 0; i < highest; i++)
	{
		if (FD_ISSET(i, f))
		{
			/* check if fd 'i' is valid... */
			//if (fcntl(i, F_GETFL) < 0)
			int nonb = 1;
			if (ioctlsocket(i, FIONBIO, &nonb) < 0)
				ircd_log(LOG_ERROR, "fd_debug: FD #%d is invalid!!!", i);
		}
	}
}
void fd_select(time_t delay)
{
	struct timeval to;
	int num, fd;
	fd_set work_read_fds;
	fd_set work_write_fds;
#ifdef _WIN32
	fd_set work_except_fds; /* only needed on windows as it may indicate a failed connect() */
#endif

	/* copy the FD sets so that our master sets are untouched */
	memcpy(&work_read_fds, &read_fds, sizeof(fd_set));
	memcpy(&work_write_fds, &write_fds, sizeof(fd_set));
#ifdef _WIN32
	memcpy(&work_except_fds, &write_fds, sizeof(fd_set));
#endif

	to.tv_sec = delay / 1000;
	to.tv_usec = (delay % 1000) * 1000;

#ifdef DEBUGMODE
	ircd_log(LOG_ERROR, "fd_select() on 0-%d...", highest_fd+1);
#endif

#ifdef _WIN32
	num = select(highest_fd + 1, &work_read_fds, &work_write_fds, &work_except_fds, &to);
#else
	num = select(highest_fd + 1, &work_read_fds, &work_write_fds, NULL, &to);
#endif
	if (num < 0)
	{
		extern void report_baderror(char *text, aClient *cptr);
		report_baderror("select %s:%s", &me);
		/* DEBUG the actual problem: */
		memcpy(&work_read_fds, &read_fds, sizeof(fd_set));
		memcpy(&work_write_fds, &write_fds, sizeof(fd_set));
		fd_debug(&work_read_fds, highest_fd+1, "read");
		fd_debug(&work_write_fds, highest_fd+1, "write");
#ifdef _WIN32
		Sleep(500);
#endif
	}

	if (num <= 0)
		return;

	for (fd = 0; fd <= highest_fd && num > 0; fd++)
	{
		FDEntry *fde;
		IOCallbackFunc iocb;
		int evflags = 0;

		fde = &fd_table[fd];
		if (!fde->is_open)
			continue;

#ifdef DEBUGMODE
		ircd_log(LOG_ERROR, "fd_select(): checking %d...", fd);
#endif

		if (FD_ISSET(fd, &work_read_fds))
			evflags |= FD_SELECT_READ;

		if (FD_ISSET(fd, &work_write_fds))
			evflags |= FD_SELECT_WRITE;

#ifdef _WIN32
		/* Exception may happen due to failed connect. Translate to write event, like on *NIX. */
		if (FD_ISSET(fd, &work_except_fds))
			evflags |= FD_SELECT_WRITE;
#endif

		if (!evflags)
			continue;

#ifdef DEBUGMODE
		ircd_log(LOG_ERROR, "fd_select(): events for %d (%d)... processing...", fd, evflags);
#endif

		if (evflags & FD_SELECT_READ)
		{
			iocb = fde->read_callback;
			if (fde->read_oneshot)
			{
				FD_CLR(fd, &read_fds);
				fde->read_callback = NULL;
			}

			if (iocb != NULL)
				iocb(fd, evflags, fde->data);

			fde->read_oneshot = 0;
		}

		if (evflags & FD_SELECT_WRITE)
		{
			iocb = fde->write_callback;
			if (fde->write_oneshot)
			{
				FD_CLR(fd, &write_fds);
				fde->write_callback = NULL;
			}

			if (iocb != NULL)
				iocb(fd, evflags, fde->data);

			fde->write_oneshot = 0;
		}

		num--;
	}
}

void fd_fork()
{
}

#endif

/***************************************************************************************
 * kqueue() backend.                                                                   *
 ***************************************************************************************/
#ifdef BACKEND_KQUEUE

#include <sys/event.h>

static int kqueue_fd = -1;
static struct kevent kqueue_events[MAXCONNECTIONS * 2];
static struct kevent kqueue_prepared[MAXCONNECTIONS * 2];
static char kqueue_enabled[MAXCONNECTIONS * 2];

void fd_fork()
{
	kqueue_fd = kqueue();
	int p;

	for (p=0; p < MAXCONNECTIONS * 2; ++p)
	{
		if (kqueue_enabled[p])
		{
			if (kevent(kqueue_fd, &kqueue_prepared[p], 1, NULL, 0, &(const struct timespec){ .tv_sec = 0, .tv_nsec = 0}) != 0)
			{
				if (ERRNO == P_EWOULDBLOCK || ERRNO == P_EAGAIN)
					continue;
					
#ifdef DEBUGMODE
				ircd_log(LOG_ERROR, "[BUG?] kevent returned %d", errno);
#endif
			}
		}
	}
}

void fd_refresh(int fd)
{
	FDEntry *fde = &fd_table[fd];

	if (kqueue_fd == -1)
	{
		kqueue_fd = kqueue();
		memset(kqueue_enabled,0,MAXCONNECTIONS*2);
	}

	kqueue_enabled[fd] = 0;
	kqueue_enabled[fd+MAXCONNECTIONS] = 0;

	if (fde->read_callback != NULL || fde->backend_flags & EVFILT_READ)
	{
		EV_SET(&kqueue_prepared[fd], (uintptr_t) fd, (short) EVFILT_READ, fde->read_callback != NULL ? EV_ADD : EV_DELETE, 0, 0, fde);
		if (kevent(kqueue_fd, &kqueue_prepared[fd], 1, NULL, 0, &(const struct timespec){ .tv_sec = 0, .tv_nsec = 0}) != 0)
		{
#ifdef DEBUGMODE
			if (ERRNO != P_EWOULDBLOCK && ERRNO != P_EAGAIN)
				ircd_log(LOG_ERROR, "[BUG?] kevent returned %d", errno);
#endif
		}
	}

	if (fde->write_callback != NULL || fde->backend_flags & EVFILT_WRITE)
	{
		EV_SET(&kqueue_prepared[fd+MAXCONNECTIONS], (uintptr_t) fd, (short) EVFILT_WRITE, fde->write_callback != NULL ? EV_ADD : EV_DELETE, 0, 0, fde);
		if (kevent(kqueue_fd, &kqueue_prepared[fd+MAXCONNECTIONS], 1, NULL, 0, &(const struct timespec){ .tv_sec = 0, .tv_nsec = 0}) != 0)
		{
#ifdef DEBUGMODE
			if (ERRNO != P_EWOULDBLOCK && ERRNO != P_EAGAIN)
				ircd_log(LOG_ERROR, "[BUG?] kevent returned %d", errno);
#endif
		}
	}

	fde->backend_flags = 0;

	if (fde->read_callback != NULL)
	{
		fde->backend_flags |= EVFILT_READ;
		kqueue_enabled[fd] = 1;

	}

	if (fde->write_callback != NULL)
	{
		fde->backend_flags |= EVFILT_WRITE;
		kqueue_enabled[fd+MAXCONNECTIONS] = 1;
	}
}

void fd_select(time_t delay)
{
	struct timespec ts;
	int num, p, revents, fd;
	struct kevent *ke;

	if (kqueue_fd == -1)
	{
		kqueue_fd = kqueue();
		memset(kqueue_enabled,0,MAXCONNECTIONS*2);
	}

	ts.tv_sec = delay / 1000;
	ts.tv_nsec = delay % 1000 * 1000000;

	num = kevent(kqueue_fd, NULL, 0, kqueue_events, MAXCONNECTIONS * 2, &ts);
	if (num <= 0)
		return;

	for (p = 0; p < num; p++)
	{
		FDEntry *fde;
		IOCallbackFunc iocb;
		int evflags = 0;

		ke = &kqueue_events[p];
		fd = ke->ident;
		revents = ke->filter;
		fde = ke->udata;

		if (revents == EVFILT_READ)
		{
			iocb = fde->read_callback;
			if (fde->read_oneshot)
				fde->read_callback = NULL;

			if (iocb != NULL)
				iocb(fd, FD_SELECT_READ, fde->data);

			fde->read_oneshot = 0;
		}

		if (revents == EVFILT_WRITE)
		{
			iocb = fde->write_callback;
			if (fde->write_oneshot)
				fde->write_callback = NULL;

			if (iocb != NULL)
				iocb(fd, FD_SELECT_WRITE, fde->data);

			fde->write_oneshot = 0;
		}
	}
}
#endif

/***************************************************************************************
 * epoll() backend.                                                                    *
 ***************************************************************************************/
#ifdef BACKEND_EPOLL

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

		ircd_log(LOG_ERROR, "[BUG] fd_refresh(): epoll_ctl returned error %d (%s) for fd %d (%s)",
			errno, STRERROR(ERRNO), fd, fde->desc);
		return;
	}

	fde->backend_flags = pflags;
}

void fd_select(time_t delay)
{
	int num, p, revents, fd;
	struct epoll_event *epfd;
#ifndef _WIN32
#define DEBUG_IOENGINE
#endif
#ifdef DEBUG_IOENGINE
	int read_callbacks = 0, write_callbacks = 0;
	struct timeval oldt, t;
	long long tdiff;
#endif
	if (epoll_fd == -1)
		epoll_fd = epoll_create(MAXCONNECTIONS);

	num = epoll_wait(epoll_fd, epfds, MAXCONNECTIONS, delay);
	if (num <= 0)
		return;

#ifdef DEBUG_IOENGINE
	gettimeofday(&oldt, NULL);
#endif

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
#ifdef DEBUG_IOENGINE
			read_callbacks++;
#endif
		}

		if (evflags & FD_SELECT_WRITE)
		{
			iocb = fde->write_callback;
			if (fde->write_oneshot)
				fde->write_callback = NULL;

			if (iocb != NULL)
				iocb(fd, evflags, fde->data);

			fde->write_oneshot = 0;
#ifdef DEBUG_IOENGINE
			write_callbacks++;
#endif
		}
#if 0
		if (((read_callbacks + write_callbacks) % 100) == 0)
		{
			/* every 100 events.. set the internal clock so we don't screw up under extreme load */
			timeofday = time(NULL) + TSoffset;
		}
#endif
	}

#ifdef DEBUG_IOENGINE
	gettimeofday(&t, NULL);
	tdiff = ((t.tv_sec - oldt.tv_sec) * 1000000) + (t.tv_usec - oldt.tv_usec);

	if (tdiff > 1000000)
	{
		sendto_realops_and_log("WARNING: Slow I/O engine or high load: fd_select() took %lld ms! read_callbacks=%d, write_callbacks=%d",
			tdiff / 1000, read_callbacks, write_callbacks);
	}
#endif
}


void fd_fork()
{
}

#endif

/***************************************************************************************
 * Poll() backend.                                                                     *
 ***************************************************************************************/
#ifdef BACKEND_POLL

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

	fde->backend_flags = pflags;
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


void fd_fork()
{
}

#endif
