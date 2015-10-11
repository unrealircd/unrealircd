/************************************************************************
 *   Unreal Internet Relay Chat Daemon, include/modversion.h
 *   (C) 2004-2005 Bram Matthys and The UnrealIRCd Team
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

#include "version.h"

/* What all this is for? Well, it's simple...
 * Example: When someone compiles a module with ssl support, but the
 * core was not compiled with ssl support, then the module will read
 * things incorrect in the struct because the module sees an extra
 * field half-way the struct but in the core that field does not exist,
 * hence all data is shifted 4 bytes causing all kinds of odd crashes,
 * memory corruption, and weird problems.
 * This is an attempt to prevent this a bit, but there are a lot more
 * options that cause binary incompatability (eg: changing nicklen),
 * we just take the most common ones...
 *
 * NOTE: On win32 we allow ssl inconsistencies because we
 *       explicitly use "padding" in the structs: we add a useless
 *       placeholder so everything is still aligned correctly.
 *       In the process of doing so, we waste several bytes per-user,
 *       but this prevents (most) binary incompatability problems
 *       making it easier for module coders to ship dll's.
 */
 #ifndef _WIN32
  #define MYTOKEN_SSL "/SSL"
 #else
  #define MYTOKEN_SSL ""
 #endif
 #if !defined(NO_FLOOD_AWAY)
  #define MYTOKEN_NOFLDAWAY "/NONFA"
 #else
  #define MYTOKEN_NOFLDAWAY ""
 #endif
 #define MYTOKEN_NEWCHF "/NOCHF"

#ifdef __GNUC__
 #if defined(__GNUC_PATCHLEVEL__)
  #define GCCVER ((__GNUC__ << 16) + (__GNUC_MINOR__ << 8) + __GNUC_PATCHLEVEL__)
 #else
  #define GCCVER ((__GNUC__ << 16) + (__GNUC_MINOR__ << 8))
 #endif
#else
 #define GCCVER 0
#endif
  

#ifdef UNREALCORE
  char our_mod_version[] = BASE_VERSION "-" PATCH1 PATCH2 PATCH3 PATCH4 PATCH6 PATCH7 PATCH8 PATCH9 \
                               MYTOKEN_SSL \
                               MYTOKEN_NOFLDAWAY MYTOKEN_NEWCHF;
  unsigned int our_compiler_version = GCCVER;
#else
  DLLFUNC char Mod_Version[] = BASE_VERSION "-" PATCH1 PATCH2 PATCH3 PATCH4 PATCH6 PATCH7 PATCH8 PATCH9 \
                               MYTOKEN_SSL \
                               MYTOKEN_NOFLDAWAY MYTOKEN_NEWCHF;
  DLLFUNC unsigned int compiler_version = GCCVER;
#endif
