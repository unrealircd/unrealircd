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
 * Example: When someone compiles a module with zip support, but the
 * core was not compiled with zip support, then the module will read
 * things incorrect in the struct because the module sees an extra
 * field half-way the struct but in the core that field does not exist,
 * hence all data is shifted 4 bytes causing all kinds of odd crashes,
 * memory corruption, and weird problems.
 * This is an attempt to prevent this a bit, but there are a lot more
 * options that cause binary incompatability (eg: changing nicklen),
 * we just take the most common ones...
 *
 * NOTE: On win32 we allow ssl and zip inconsistencies because we
 *       explicitly use "padding" in the structs: we add a useless
 *       placeholder so everything is still aligned correctly.
 *       In the process of doing so, we waste several bytes per-user,
 *       but this prevents (most) binary incompatability problems
 *       making it easier for module coders to ship dll's.
 */
 #if defined(USE_SSL) && !defined(_WIN32)
  #define MYTOKEN_SSL "/SSL"
 #else
  #define MYTOKEN_SSL ""
 #endif
 #if defined(ZIP_LINKS) && !defined(_WIN32)
  #define MYTOKEN_ZIP "/ZIP"
 #else
  #define MYTOKEN_ZIP ""
 #endif
 #if defined(NOSPOOF)
  #define MYTOKEN_NOSPOOF "/NOSPF"
 #else
  #define MYTOKEN_NOSPOOF ""
 #endif
 #if !defined(EXTCMODE)
  #define MYTOKEN_EXTCMODE "/NOEXTC"
 #else
  #define MYTOKEN_EXTCMODE ""
 #endif
 #if !defined(JOINTHROTTLE)
  #define MYTOKEN_JOINTHROTTLE "/NOJTHR"
 #else
  #define MYTOKEN_JOINTHROTTLE ""
 #endif
 #if !defined(NO_FLOOD_AWAY)
  #define MYTOKEN_NOFLDAWAY "/NONFA"
 #else
  #define MYTOKEN_NOFLDAWAY ""
 #endif
 #if !defined(NEWCHFLOODPROT)
  #define MYTOKEN_NEWCHF "/NOCHF"
 #else
  #define MYTOKEN_NEWCHF ""
 #endif
 #ifdef INET6
  #define MYTOKEN_INET6 "/IPV6"
 #else
  #define MYTOKEN_INET6 ""
 #endif

#ifdef UNREALCORE
  char our_mod_version[] = BASE_VERSION PATCH1 PATCH2 PATCH3 PATCH4 PATCH5 PATCH6 PATCH7 PATCH8 PATCH9 \
                               MYTOKEN_SSL MYTOKEN_ZIP MYTOKEN_NOSPOOF MYTOKEN_EXTCMODE MYTOKEN_JOINTHROTTLE \
                               MYTOKEN_NOFLDAWAY MYTOKEN_NEWCHF MYTOKEN_INET6;
#else
  DLLFUNC char Mod_Version[] = BASE_VERSION PATCH1 PATCH2 PATCH3 PATCH4 PATCH5 PATCH6 PATCH7 PATCH8 PATCH9 \
                               MYTOKEN_SSL MYTOKEN_ZIP MYTOKEN_NOSPOOF MYTOKEN_EXTCMODE MYTOKEN_JOINTHROTTLE \
                               MYTOKEN_NOFLDAWAY MYTOKEN_NEWCHF MYTOKEN_INET6;
#endif
