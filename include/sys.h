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
#ifdef ISC202
#include <net/errno.h>
#else
# ifndef _WIN32
#include <sys/errno.h>
# else
#include <errno.h>
# endif
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
#ifdef SSL
#include <openssl/ssl.h>
#endif
#ifdef INET6
#ifndef _WIN32
#include <netinet/in.h>
#include <sys/socket.h>
#else
#include <winsock2.h>
#include <ws2tcpip.h>
#endif
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
#endif
#ifdef DEBUGMODE
// #define ircsprintf sprintf
//#define ircvsprintf vsprintf
#endif


/*
 *  IPv4 or IPv6 structures?
 */

#ifdef INET6

# define AND16(x) ((x)[0]&(x)[1]&(x)[2]&(x)[3]&(x)[4]&(x)[5]&(x)[6]&(x)[7]&(x)[8]&(x)[9]&(x)[10]&(x)[11]&(x)[12]&(x)[13]&(x)[14]&(x)[15])
# define WHOSTENTP(x) ((x)[0]|(x)[1]|(x)[2]|(x)[3]|(x)[4]|(x)[5]|(x)[6]|(x)[7]|(x)[8]|(x)[9]|(x)[10]|(x)[11]|(x)[12]|(x)[13]|(x)[14]|(x)[15])

# define	AFINET		AF_INET6
# define	SOCKADDR_IN	sockaddr_in6
# define	SOCKADDR	sockaddr
# define	SIN_FAMILY	sin6_family
# define	SIN_PORT	sin6_port
# define	SIN_ADDR	sin6_addr
# define	S_ADDR		s6_addr
# define	IN_ADDR		in6_addr

// # ifndef uint32_t
//#  define uint32_t __u32
// # endif

# define MYDUMMY_SIZE 128
char mydummy[MYDUMMY_SIZE];
char mydummy2[MYDUMMY_SIZE];

# if defined(linux) || defined(__NetBSD__) || defined(__FreeBSD__) || defined(bsdi)
#  ifndef s6_laddr
#   define s6_laddr        s6_addr32
#  endif
# endif

# if defined(linux)
static const struct in6_addr in6addr_any = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0
};

# endif

# define IRCDCONF_DELIMITER '%'

#else
# define	AFINET		AF_INET
# define	SOCKADDR_IN	sockaddr_in
# define	SOCKADDR	sockaddr
# define	SIN_FAMILY	sin_family
# define	SIN_PORT	sin_port
# define	SIN_ADDR	sin_addr
# define	S_ADDR		s_addr
# define	IN_ADDR		in_addr

# define WHOSTENTP(x) (x)
# define IRCDCONF_DELIMITER ':'
#endif

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
#else

/* IO and Error portability macros */
#define READ_SOCK(fd, buf, len) recv((fd), (buf), (len), 0)
#define WRITE_SOCK(fd, buf, len) send((fd), (buf), (len), 0)
#define CLOSE_SOCK(fd) closesocket(fd)
#define IOCTL(x, y, z) ioctlsocket((x), (y), (z))
#define ERRNO WSAGetLastError()
#define STRERROR(x) nt_strerror(x)
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
#endif

#endif /* __sys_include__ */
