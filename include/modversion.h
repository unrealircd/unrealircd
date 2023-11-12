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

/* At UnrealIRCd we don't have a stable module ABI.
 * We check for version nowadays, but actually there are many more
 * ways to cause binary interface screwups, like using the git
 * version and then have a different include/struct.h on your
 * running unrealircd compared to your modules, with members shifted
 * or reordered and the like. Fun!
 */

#ifdef UNREALCORE
  char our_mod_version[] = BASE_VERSION "-" PATCH1 PATCH2 PATCH3 PATCH4 PATCH6 PATCH7 PATCH8 PATCH9;
#else
  DLLFUNC char Mod_Version[] = BASE_VERSION "-" PATCH1 PATCH2 PATCH3 PATCH4 PATCH6 PATCH7 PATCH8 PATCH9;
#endif
