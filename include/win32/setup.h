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
#define CONFDIR "conf"
#define MODULESDIR "modules"
#define LOGDIR "logs"
#define PERMDATADIR "data"
#define CACHEDIR "cache"
#define TMPDIR "tmp"
#define PIDFILE PERMDATADIR"/unrealircd.pid"
#define NO_U_TYPES
#define NEED_U_INT32_T
#define PREFIX_AQ
#define LIST_SHOW_MODES
#ifndef mode_t
/*
  Needed in s_conf.c for the third argument of open(3p).

  Should be an int because of http://msdn.microsoft.com/en-us/library/z0kc8e3z(VS.71).aspx
 */
#define mode_t int
#endif

/*
  make up for win32 (and win64?) users not being able to run ./configure.
 */
#ifndef intptr_t
#define intptr_t long
#endif

/* Generation version number (e.g.: 3 for Unreal3*) */
#define UNREAL_VERSION_GENERATION 4

/* Major version number (e.g.: 2 for Unreal3.2*) */
#define UNREAL_VERSION_MAJOR 0

/* Minor version number (e.g.: 1 for Unreal3.2.1) */
#define UNREAL_VERSION_MINOR 2

/* Version suffix such as a beta marker or release candidate marker. (e.g.:
   -rcX for unrealircd-3.2.9-rcX) */
#define UNREAL_VERSION_SUFFIX ""

#endif
