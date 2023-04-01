/*
 * UnrealIRCd, src/dispatch.c
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

/* Some specials here, for this file.. */

/* Do we even support this, poll on Windows? */
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
#ifndef _WIN32
#include <sys/file.h>
#include <sys/ioctl.h>
#endif

/* Not sure if this is suitable for production,
 * but let's turn it on for U6 development.
 */
//#define DETECT_HIGH_CPU

/***************************************************************************************
 * Backend-independent functions.  fd_setselect() and friends                          *
 ***************************************************************************************/
void fd_setselect(int fd, int flags, IOCallbackFunc iocb, void *data)
{
	FDEntry *fde;
	int changed = 0;
#if 0
	unreal_log(ULOG_DEBUG, "io", "IO_DEBUG_FD_SETSELECT", NULL,
	           "fd_setselect(): fd $fd flags $fd_flags function $function_pointer",
	           log_data_integer("fd", fd),
	           log_data_integer("fd_flags", flags),
	           log_data_integer("function_pointer", (long long)iocb));
#endif
	if ((fd < 0) || (fd >= MAXCONNECTIONS))
	{
		unreal_log(ULOG_ERROR, "io", "BUG_FD_SETSELECT_OUT_OF_RANGE", NULL,
		           "[BUG] trying to modify fd $fd in fd table, but MAXCONNECTIONS is $maxconnections",
		           log_data_integer("fd", fd),
		           log_data_integer("maxconnections", MAXCONNECTIONS));
#ifdef DEBUGMODE
		abort();
#endif
		return;
	}

	fde = &fd_table[fd];
	fde->data = data;

	if (flags & FD_SELECT_READ)
	{
		if (fde->read_callback != iocb)
		{
			fde->read_callback = iocb;
			changed = 1;
		}
	}
	if (flags & FD_SELECT_WRITE)
	{
		if (fde->write_callback != iocb)
		{
			fde->write_callback = iocb;
			changed = 1;
		}
	}

	// This is efficient, but.. there are places which do two fd_setselect(),
	// it would be nice if we can merge this into one syscall..
	if (changed)
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
			{
				unreal_log(ULOG_ERROR, "io", "FD_DEBUG", NULL,
					   "[BUG] fd_debug: fd $fd is invalid!!!",
					   log_data_integer("fd", i));
			}
		}
	}
}
void fd_select(int delay)
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

	memset(&to, 0, sizeof(to));
	to.tv_sec = delay / 1000;
	to.tv_usec = (delay % 1000) * 1000;

#ifdef _WIN32
	num = select(highest_fd + 1, &work_read_fds, &work_write_fds, &work_except_fds, &to);
#else
	num = select(highest_fd + 1, &work_read_fds, &work_write_fds, NULL, &to);
#endif
	if (num < 0)
	{
		unreal_log(ULOG_FATAL, "io", "SELECT_ERROR", NULL,
		           "select() returned error ($socket_error) -- SERIOUS TROUBLE!",
		           log_data_socket_error(-1));
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

		if (evflags & FD_SELECT_READ)
		{
			iocb = fde->read_callback;

			if (iocb != NULL)
				iocb(fd, evflags, fde->data);
		}

		if (evflags & FD_SELECT_WRITE)
		{
			iocb = fde->write_callback;

			if (iocb != NULL)
				iocb(fd, evflags, fde->data);
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
				unreal_log(ULOG_ERROR, "io", "KEVENT_FAILED", NULL,
				           "[io] fd_fork(): kevent returned error: $system_error",
				           log_data_string("system_error", strerror(errno)));
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
			{
				int save_err = errno;
				unreal_log(ULOG_ERROR, "io", "KEVENT_FAILED_REFRESH", NULL,
				           "fd_refresh(): kevent returned error for fd $fd ($fd_action) ($callback): $system_error",
				           log_data_string("system_error", strerror(save_err)),
				           log_data_integer("fd", fd),
				           log_data_string("fd_action", (fde->read_callback ? "add" : "delete")),
				           log_data_string("callback", "read_callback"));
			}
#endif
		}
	}

	if (fde->write_callback != NULL || fde->backend_flags & EVFILT_WRITE)
	{
		EV_SET(&kqueue_prepared[fd+MAXCONNECTIONS], (uintptr_t) fd, (short) EVFILT_WRITE, fde->write_callback != NULL ? EV_ADD : EV_DELETE, 0, 0, fde);
		if (kevent(kqueue_fd, &kqueue_prepared[fd+MAXCONNECTIONS], 1, NULL, 0, &(const struct timespec){ .tv_sec = 0, .tv_nsec = 0}) != 0)
		{
#ifdef DEBUGMODE
			if (ERRNO != P_EWOULDBLOCK && ERRNO != P_EAGAIN && fde->write_callback)
			{
				int save_err = errno;
				unreal_log(ULOG_ERROR, "io", "KEVENT_FAILED_REFRESH", NULL,
				           "[io] fd_refresh(): kevent returned error for fd $fd ($fd_action) ($callback): $system_error",
				           log_data_string("system_error", strerror(save_err)),
				           log_data_integer("fd", fd),
				           log_data_string("fd_action", "add"),
				           log_data_string("callback", "write_callback"));
			}
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

void fd_select(int delay)
{
	struct timespec ts;
	int num, p, revents, fd;
	struct kevent *ke;

	if (kqueue_fd == -1)
	{
		kqueue_fd = kqueue();
		memset(kqueue_enabled,0,MAXCONNECTIONS*2);
	}

	memset(&ts, 0, sizeof(ts));
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

			if (iocb != NULL)
				iocb(fd, FD_SELECT_READ, fde->data);
		}

		if (revents == EVFILT_WRITE)
		{
			iocb = fde->write_callback;

			if (iocb != NULL)
				iocb(fd, FD_SELECT_WRITE, fde->data);
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
	int op = -1;

	if (epoll_fd == -1)
		epoll_fd = epoll_create(MAXCONNECTIONS);

	if (fde->read_callback)
		pflags |= EPOLLIN;

	if (fde->write_callback)
		pflags |= EPOLLOUT;

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

	memset(&ep_event, 0, sizeof(ep_event));
	ep_event.events = pflags;
	ep_event.data.ptr = fde;

	if (epoll_ctl(epoll_fd, op, fd, &ep_event) != 0)
	{
		int save_errno = errno;
		if ((save_errno == P_EWOULDBLOCK) || (save_errno == P_EAGAIN))
			return;

		unreal_log(ULOG_ERROR, "io", "EPOLL_CTL_FAILED", NULL,
			   "[io] fd_refresh(): epoll_ctl returned error for fd $fd ($fd_description): $system_error",
			   log_data_string("system_error", strerror(save_errno)),
			   log_data_integer("fd", fd),
			   log_data_string("fd_description", fde->desc));
		return;
	}

	fde->backend_flags = pflags;
}

void fd_select(int delay)
{
	int num, p, revents, fd;
	struct epoll_event *epfd;
#ifdef DETECT_HIGH_CPU
	int read_callbacks = 0, write_callbacks = 0;
	struct timeval oldt, t;
	long long tdiff;
#endif
	if (epoll_fd == -1)
		epoll_fd = epoll_create(MAXCONNECTIONS);

	num = epoll_wait(epoll_fd, epfds, MAXCONNECTIONS, delay);
	if (num <= 0)
		return;

#ifdef DETECT_HIGH_CPU
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

			if (iocb != NULL)
				iocb(fd, evflags, fde->data);

#ifdef DETECT_HIGH_CPU
			read_callbacks++;
#endif
		}

		if (evflags & FD_SELECT_WRITE)
		{
			iocb = fde->write_callback;

			if (iocb != NULL)
				iocb(fd, evflags, fde->data);

#ifdef DETECT_HIGH_CPU
			write_callbacks++;
#endif
		}
#if 0
		if (((read_callbacks + write_callbacks) % 100) == 0)
		{
			/* every 100 events.. set the internal clock so we don't screw up under extreme load */
			timeofday = time(NULL);
		}
#endif
	}

#ifdef DETECT_HIGH_CPU
	gettimeofday(&t, NULL);
	tdiff = ((t.tv_sec - oldt.tv_sec) * 1000000) + (t.tv_usec - oldt.tv_usec);

	if (tdiff > 1000000)
	{
		unreal_log(ULOG_WARNING, "io", "HIGH_LOAD", NULL,
		           "HIGH CPU LOAD! fd_select() took $time_msec msec "
		           "(read: $num_read_callbacks, write: $num_write_callbacks)",
		           log_data_integer("time_msec", tdiff/1000),
		           log_data_integer("num_read_callbacks", read_callbacks),
		           log_data_integer("num_write_callbacks", write_callbacks));
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

void fd_select(int delay)
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

			if (iocb != NULL)
				iocb(fd, evflags, fde->data);
		}

		if (evflags & FD_SELECT_WRITE)
		{
			iocb = fde->write_callback;
			if (iocb != NULL)
				iocb(fd, evflags, fde->data);
		}
	}
}


void fd_fork()
{
}

#endif
