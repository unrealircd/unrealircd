/*
 *   Unreal Internet Relay Chat Daemon, include/types.h
 *   (C) 2003 The UnrealIRCd Team
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

#ifndef TYPES_H
#define TYPES_H

typedef void                    (*vFP)();       /* Void function pointer */
typedef int                     (*iFP)();       /* Integer function pointer */
typedef char                    (*cFP)();       /* char * function pointer */

#ifndef MODVAR
 #if defined(MODULE_COMPILE) && defined(_WIN32)
  #define MODVAR __declspec(dllimport)
 #else
  #define MODVAR
 #endif
#endif

#ifndef MODFUNC
 #ifdef _WIN32
  #define MODFUNC __declspec(dllexport)
 #else
  #define MODFUNC
 #endif
#endif

#endif
