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

#ifndef GOT_STRCASECMP
#define	strcasecmp	mycmp
#define	strncasecmp	myncmp
#endif

#ifdef NOINDEX
#define   index   strchr
#define   rindex  strrchr
/*
extern	char	*index PROTO((char *, char));
extern	char	*rindex PROTO((char *, char));
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
#if !defined(DEBUGMODE) 
# ifndef _WIN32
#  define MyFree(x)	if ((x) != NULL) free(x)
# else
#  define MyFree(x)       if ((x) != NULL) GlobalFree(x)
# endif
#else
#define	free(x)		MyFree(x)
#endif
#ifdef NEXT
#define VOIDSIG int	/* whether signal() returns int of void */
#else
#define VOIDSIG void	/* whether signal() returns int of void */
#endif

#ifdef _SOLARIS
#define OPT_TYPE char	/* opt type for get/setsockopt */
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
extern	VOIDSIG	dummy();
#endif

#ifdef	DYNIXPTX
#define	NO_U_TYPES
typedef unsigned short n_short;         /* short as received from the net */
typedef unsigned long   n_long;         /* long as received from the net */
typedef unsigned long   n_time;         /* ms since 00:00 GMT, byte rev */
#define _NETINET_IN_SYSTM_INCLUDED
#endif

#ifdef	NO_U_TYPES
typedef	unsigned char	u_char;
typedef	unsigned short	u_short;
typedef	unsigned long	u_long;
typedef	unsigned int	u_int;
#endif

#ifdef _WIN32
#define MYOSNAME "Win32"
#endif
#ifdef DEBUGMODE
#define ircsprintf sprintf
#define ircvsprintf vsprintf
#endif


/*
 *  IPv4 or IPv6 structures?
 */

#ifdef INET6

# define AND16(x) ((x)[0]&(x)[1]&(x)[2]&(x)[3]&(x)[4]&(x)[5]&(x)[6]&(x)[7]&(x)[8]&(x)[9]&(x)[10]&(x)[11]&(x)[12]&(x)[13]&(x)[14]&(x)[15])
static unsigned char minus_one[]={ 255, 255, 255, 255, 255, 255, 255, 255, 255,
					255, 255, 255, 255, 255, 255, 255, 0};
# define WHOSTENTP(x) ((x)[0]|(x)[1]|(x)[2]|(x)[3]|(x)[4]|(x)[5]|(x)[6]|(x)[7]|(x)[8]|(x)[9]|(x)[10]|(x)[11]|(x)[12]|(x)[13]|(x)[14]|(x)[15])

# define	AFINET		AF_INET6
# define	SOCKADDR_IN	sockaddr_in6
# define	SOCKADDR	sockaddr
# define	SIN_FAMILY	sin6_family
# define	SIN_PORT	sin6_port
# define	SIN_ADDR	sin6_addr
# define	S_ADDR		s6_addr
# define	IN_ADDR		in6_addr

# ifndef uint32_t
#  define uint32_t __u32
# endif

# define MYDUMMY_SIZE 128
char mydummy[MYDUMMY_SIZE];
char mydummy2[MYDUMMY_SIZE];

# if defined(linux) || defined(__NetBSD__) || defined(__FreeBSD__) || defined(bsdi)
#  ifndef s6_laddr
#   define s6_laddr        s6_addr32
#  endif
# endif

# if defined(linux)
static const struct in6_addr in6addr_any={ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
						0, 0, 0, 0, 0};
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
   
#endif /* __sys_include__ */
