/*
 *   Unreal Internet Relay Chat Daemon, include/sys.h
 *   Copyright (C) 1990 University of Oulu, Computing Center
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
 *   
 *   $Id$
 */

#ifndef	__sys_include__
#define __sys_include__

/* PATH_MAX */
#include <limits.h>

#ifdef ISC202
#include <net/errno.h>
#else
#include <errno.h>
#endif
#include "setup.h"
#include <stdio.h>
#include <sys/types.h>
#ifndef _WIN32
#include <sys/param.h>
#else
#include <stdarg.h>
#endif

#ifdef	UNISTDH
#include <unistd.h>
#endif
#ifdef	STDLIBH
#include <stdlib.h>
#endif
#ifdef	STRINGSH
#include <strings.h>
#else
# ifdef	STRINGH
# include <string.h>
# endif
#endif

/* get intptr_t if the system provides it -- otherwise, ./configure will define it for us */
#ifdef HAVE_STDINT_H
#include <stdint.h>
#else
#ifdef HAVE_INTTYPES_H
#include <inttypes.h>
#endif /* HAVE_INTTYPES_H */
#endif /* HAVE_STDINT_H */

#ifdef SSL
#include <openssl/ssl.h>
#endif
#ifndef _WIN32
#include <netinet/in.h>
#include <sys/socket.h>
#else
#define _WIN32_WINNT 0x0501
#include <winsock2.h>
#include <ws2tcpip.h>
#endif
#ifndef GOT_STRCASECMP
#define	strcasecmp	mycmp
#define	strncasecmp	myncmp
#endif

#ifdef NOINDEX
#define   index   strchr
#define   rindex  strrchr
/*
extern	char	*index(char *, char);
extern	char	*rindex(char *, char);
*/
#endif
#ifdef NOBCOPY
#define bcopy(x,y,z)	memcpy(y,x,z)
#define bcmp(x,y,z)	memcmp(x,y,z)
#define bzero(p,s)	memset(p,0,s)
#endif

#ifdef AIX
#include <sys/select.h>
#endif
#if defined(HPUX )|| defined(AIX) || defined(_WIN32)
#include <time.h>
#ifdef AIX
#include <sys/time.h>
#endif
#else
#include <sys/time.h>
#endif
#ifdef NEXT
#define VOIDSIG int		/* whether signal() returns int of void */
#else
#define VOIDSIG void		/* whether signal() returns int of void */
#endif

#ifdef _SOLARIS
#define OPT_TYPE char		/* opt type for get/setsockopt */
#else
#define OPT_TYPE void
#endif

/*
 * Different name on NetBSD, FreeBSD, and BSDI
 */
#if defined(__NetBSD__) || defined(__FreeBSD__) || defined(__bsdi__) || defined(__linux__) || defined(__APPLE__)
#define dn_skipname  __dn_skipname
#endif

/*
 * Mac OS X Tiger Support (Intel Only)
 */
#if defined(macosx) || defined(__APPLE__)
#define OSXTIGER
#endif

#ifndef _WIN32
extern VOIDSIG dummy();
#endif

#ifdef	NO_U_TYPES
typedef unsigned char u_char;
typedef unsigned short u_short;
typedef unsigned long u_long;
typedef unsigned int u_int;
#endif

#ifdef _WIN32
#define MYOSNAME OSName
extern char OSName[256];
#define PATH_MAX MAX_PATH
#else
#define MYOSNAME getosname()
#endif
#ifdef DEBUGMODE
#endif

#ifdef _WIN32
typedef unsigned short u_int16_t;
typedef unsigned __int64 uint64_t;
typedef unsigned __int32 uint32_t;
#endif

# define MYDUMMY_SIZE 128

/*
 * Socket, File, and Error portability macros
 */
#ifndef _WIN32
#define SET_ERRNO(x) errno = x
#define READ_SOCK(fd, buf, len) read((fd), (buf), (len))
#define WRITE_SOCK(fd, buf, len) write((fd), (buf), (len))
#define CLOSE_SOCK(fd) close(fd)
#define IOCTL(x, y, z) ioctl((x), (y), (z))
#define ERRNO errno
#define STRERROR(x) strerror(x)

/* error constant portability */
#define P_EMFILE        EMFILE
#define P_ENOBUFS       ENOBUFS
#define P_EWOULDBLOCK   EWOULDBLOCK
#define P_EAGAIN        EAGAIN
#define P_EINPROGRESS   EINPROGRESS
#define P_EWORKING		EINPROGRESS
#define P_EINTR         EINTR
#define P_ETIMEDOUT     ETIMEDOUT
#define P_ENOTSOCK	ENOTSOCK
#define P_EIO		EIO
#define P_ECONNABORTED	ECONNABORTED
#define P_ECONNRESET	ECONNRESET
#define P_ENOTCONN	ENOTCONN
#define P_EMSGSIZE	EMSGSIZE
#else
/* WIN32 */

#define NETDB_INTERNAL  -1  /* see errno */
#define NETDB_SUCCESS   0   /* no problem */

/* IO and Error portability macros */
#define READ_SOCK(fd, buf, len) recv((fd), (buf), (len), 0)
#define WRITE_SOCK(fd, buf, len) send((fd), (buf), (len), 0)
#define CLOSE_SOCK(fd) closesocket(fd)
#define IOCTL(x, y, z) ioctlsocket((x), (y), (z))
#define ERRNO WSAGetLastError()
#define STRERROR(x) sock_strerror(x)
#define SET_ERRNO(x) WSASetLastError(x)
/* Error constant portability */
#define P_EMFILE        WSAEMFILE
#define P_ENOBUFS       WSAENOBUFS
#define P_EWOULDBLOCK   WSAEWOULDBLOCK
#define P_EAGAIN        WSAEWOULDBLOCK
#define P_EINPROGRESS   WSAEINPROGRESS
#define P_EWORKING		WSAEWOULDBLOCK
#define P_EINTR         WSAEINTR
#define P_ETIMEDOUT     WSAETIMEDOUT
#define P_ENOTSOCK	WSAENOTSOCK
#define P_EIO		EIO
#define P_ECONNABORTED	WSAECONNABORTED
#define P_ECONNRESET	WSAECONNRESET
#define P_ENOTCONN	WSAENOTCONN
#define P_EMSGSIZE	WSAEMSGSIZE
#endif

#ifndef __GNUC__
#define __attribute__(x) /* nothing */
#endif

#endif /* __sys_include__ */
