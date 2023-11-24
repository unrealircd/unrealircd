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
#include <process.h>
#endif
#include <stdlib.h>
#ifndef _WIN32
#include <unistd.h>
#include <strings.h>
#include <sys/resource.h>
#endif

/* get intptr_t if the system provides it -- otherwise, ./configure will define it for us */
#ifdef HAVE_STDINT_H
#include <stdint.h>
#else
#ifdef HAVE_INTTYPES_H
#include <inttypes.h>
#endif /* HAVE_INTTYPES_H */
#endif /* HAVE_STDINT_H */

#ifndef _WIN32
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <arpa/inet.h>
#else
#include <winsock2.h>
#include <ws2tcpip.h>
#include <afunix.h>
#endif

#ifdef HAVE_TCP_INFO
 #include <netinet/tcp.h>
#endif

#ifndef _WIN32
#include <sys/time.h>
#endif
#include <time.h>

#ifndef _WIN32
#include <sys/wait.h>
#ifndef WEXITSTATUS
#define WEXITSTATUS(stat_val) ((unsigned)(stat_val) >> 8)
#endif
#ifndef WIFEXITED
#define WIFEXITED(stat_val) (((stat_val) & 255) == 0)
#endif
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
extern void dummy();
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
#define getpid _getpid
#else
#define MYOSNAME getosname()
#endif

#define MYDUMMY_SIZE 128

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

#undef FORMAT_STRING
#if _MSC_VER >= 1400
# include <sal.h>
# if _MSC_VER > 1400
#  define FORMAT_STRING(p) _Printf_format_string_ p
# else
#  define FORMAT_STRING(p) __format_string p
# endif /* FORMAT_STRING */
#else
# define FORMAT_STRING(p) p
#endif /* _MSC_VER */

/* A normal abort() on windows causes the crucial stack frame to be missing
 * from the stack trace, IOTW: you don't see where abort() was called!
 * It's silly but this works:
 */
#ifdef _WIN32
 #define abort()  do { char *crash = NULL; *crash = 'x'; exit(1); } while(0)
#endif

#ifndef SOMAXCONN
# define LISTEN_SIZE	(5)
#else
# define LISTEN_SIZE	(SOMAXCONN)
#endif

/*
 * Try and find the correct name to use with getrlimit() for setting the max.
 * number of files allowed to be open by this process.
 */
#ifdef RLIMIT_FDMAX
# define RLIMIT_FD_MAX   RLIMIT_FDMAX
#else
# ifdef RLIMIT_NOFILE
#  define RLIMIT_FD_MAX RLIMIT_NOFILE
# else
#  ifdef RLIMIT_OPEN_MAX
#   define RLIMIT_FD_MAX RLIMIT_OPEN_MAX
#  else
#   undef RLIMIT_FD_MAX
#  endif
# endif
#endif

#ifdef NATIVE_BIG_ENDIAN
 #if defined(__FreeBSD__) || defined(__DragonFly__) || defined(__NetBSD__)
  #include <sys/endian.h>
  #define bswap_64 bswap64
  #define bswap_32 bswap32
  #define bswap_16 bswap16
 #else
  #include <byteswap.h>
 #endif
#endif

#endif /* __sys_include__ */
