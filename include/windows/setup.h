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

#undef  SYSSYSLOGH
#define NOINDEX
#undef  TIMES_2
#undef  GETRUSAGE_2
#define CONFDIR "conf"
#define MODULESDIR "modules"
#define LOGDIR "logs"
#define PERMDATADIR "data"
#define CACHEDIR "cache"
#define TMPDIR "tmp"
#define PIDFILE PERMDATADIR"/unrealircd.pid"
#define CONTROLFILE PERMDATADIR"/unrealircd.ctl"
#define NO_U_TYPES
#define NEED_U_INT32_T
#define strcasecmp _stricmp
#define strncasecmp _strnicmp
#define HAVE_EXPLICIT_BZERO
#define explicit_bzero(a,b) SecureZeroMemory(a,b)

/* mode_t: Needed in s_conf.c for the third argument of open(3p).
 * Should be an int because of http://msdn.microsoft.com/en-us/library/z0kc8e3z(VS.71).aspx
 */
#define mode_t int

/* We don't use any of the wincrypt stuff and this silences
 * a warning emitted by LibreSSL:
 */
#define NOCRYPT

/* We require Windows 7 or later */
#define NTDDI_VERSION 0x06010000
#define _WIN32_WINNT 0x0601

/* Generation version number (e.g.: 3 for Unreal3*) */
#define UNREAL_VERSION_GENERATION 6

/* Major version number (e.g.: 2 for Unreal3.2*) */
#define UNREAL_VERSION_MAJOR 0

/* Minor version number (e.g.: 1 for Unreal3.2.1) */
#define UNREAL_VERSION_MINOR 3

/* Version suffix such as a beta marker or release candidate marker. (e.g.:
   -rcX for unrealircd-3.2.9-rcX) */
#define UNREAL_VERSION_SUFFIX ""

#endif
