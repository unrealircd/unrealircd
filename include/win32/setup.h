/************************************************************************
 *   IRC - Internet Relay Chat, include/win32/setup.h
 *   Copyright (C) 1999 Carsten Munk
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

#ifndef __setup_include__
#define __setup_include__
#undef  PARAMH
#undef  UNISTDH
#define STRINGH
#undef  STRINGSH
#define STDLIBH
#undef  STDDEFH
#undef  SYSSYSLOGH
#define NOINDEX
#define NOBCOPY
#define NEED_STRTOKEN
#undef  NEED_STRTOK
#undef  NEED_INET_ADDR
#undef  NEED_INET_NTOA
#define NEED_INET_NETOF
#define GETTIMEOFDAY
#undef  LRAND48
#define MALLOCH <malloc.h>
#undef  NBLOCK_POSIX
#undef  POSIX_SIGNALS
#undef  TIMES_2
#undef  GETRUSAGE_2
#define SPATH "."
#define DPATH "."
#define DOMAINNAME "irc.net"
#define NO_U_TYPES
#define NEED_U_INT32_T
#define PREFIX_AQ
#endif
