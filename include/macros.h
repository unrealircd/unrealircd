/************************************************************************
 *   Unreal Internet Relay Chat Daemon, include/macros.h
 *   Copyright (c) 2004 Dominick Meglio & The UnrealIRCd Team
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

#include "setup.h"

/* Calculate the size of an array */
#define ARRAY_SIZEOF(x) (sizeof((x))/sizeof((x)[0]))

/* Allocate a dynamic local variable */
#if defined(HAVE_C99_VARLEN_ARRAY)
#define DYN_LOCAL(type, name, size) type name[size]
#define DYN_FREE(name)
#elif defined(HAVE_ALLOCA)
#define DYN_LOCAL(type, name, size) type *name = (size ? alloca(size) : NULL)
#define DYN_FREE(name)
#else
#define DYN_LOCAL(type, name, size) type *name = (size ? malloc(size) : NULL)
#define DYN_FREE(name) (name ? free(name) : 0)
#endif
