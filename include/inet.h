/*
 * Copyright (c) 1983 Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that: (1) source distributions retain this entire copyright
 * notice and comment, and (2) distributions including binaries display
 * the following acknowledgement:  ``This product includes software
 * developed by the University of California, Berkeley and its contributors''
 * in the documentation or other materials provided with the distribution
 * and in all advertising materials mentioning features or use of this
 * software. Neither the name of the University nor the names of its
 * contributors may be used to endorse or promote products derived
 * from this software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 * $Id$
 *
 *	@(#)inet.h	5.4 (Berkeley) 6/1/90
 */

/* External definitions for functions in inet(3) */
#include "config.h"		/* for system definitions */

#ifdef	__alpha
#define	__u_l	unsigned int
#else
#define	__u_l	unsigned long
#endif

extern int inet_pton(int af, const char *src, void *dst);
extern const char *inet_ntop(int af, const void *src, char *dst, size_t cnt);

#ifdef __STDC__
# ifndef _WIN32
extern __u_l inet_addr(char *);
extern char *inet_ntoa(struct in_addr);
extern int inet_aton(const char *, struct in_addr *);
extern int  inet_netof(struct in_addr);
# endif
extern __u_l inet_makeaddr(int, int);
extern __u_l inet_network(char *);
extern __u_l inet_lnaof(struct in_addr);
#else
# ifndef _WIN32
extern __u_l inet_addr();
extern char *inet_ntoa();
# endif
#ifndef HPUX
extern __u_l inet_makeaddr();
#endif
#endif
#ifndef  HPUX
extern __u_l inet_network();
extern __u_l inet_lnaof();
#endif
#undef __u_l

#ifndef HAVE_STRUCT_SOCKADDR_STORAGE
#ifdef SOCKADDR_IN_HAS_LEN /* BSD style sockaddr_storage for BSD style
            sockaddr_in */
struct sockaddr_storage {
  unsigned char ss_len;
  sa_family_t ss_family;
  char __ss_pad1[((sizeof(int64_t)) - sizeof(unsigned char) -
      sizeof(sa_family_t) )];
  int64_t __ss_align;
  char __ss_pad2[(128 - sizeof(unsigned char) - sizeof(sa_family_t) -
      ((sizeof(int64_t)) - sizeof(unsigned char) -
       sizeof(sa_family_t)) - (sizeof(int64_t)))];
};
#else /* Linux style for everything else (for now) */
struct sockaddr_storage
{
  sa_family_t ss_family;
  u_int32_t __ss_align;
  char __ss_padding[(128 - (2 * sizeof (u_int32_t)))];
};
#endif /* SOCKADDR_IN_HAS_LEN */
#endif /* HAVE_STRUCT_SOCKADDR_STORAGE */

#ifndef INADDR_NONE
#define INADDR_NONE 0xffffffff
#endif
