/*
 *   Unreal Internet Relay Chat Daemon, src/modules/no-implicit-names.c
 *   (C) 2023 Valware and the UnrealIRCd Team
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

#include "unrealircd.h"

ModuleHeader MOD_HEADER
  = {
	"no-implicit-names",
	"1.0",
	"Opt out of receiving an implicit NAMES list on JOIN", 
	"UnrealIRCd Team",
	"unrealircd-6",
};

#define NO_IMPLICIT_NAMES_CAP_DRAFT "draft/no-implicit-names"
#define NO_IMPLICIT_NAMES_CAP "no-implicit-names"

long CAP_NO_IMPLICIT_NAMES_DRAFT = 0L;
long CAP_NO_IMPLICIT_NAMES = 0L;

MOD_INIT()
{
	/** We only add the draft version for now */
	ClientCapabilityInfo cap;
	memset(&cap, 0, sizeof(cap));
	cap.name = NO_IMPLICIT_NAMES_CAP_DRAFT;
	if (!ClientCapabilityAdd(modinfo->handle, &cap, &CAP_NO_IMPLICIT_NAMES_DRAFT))
	{
		return MOD_FAILED;
	}

	/** This is for the future :D */
	/**
	memset(&cap, 0, sizeof(cap));
	cap.name = NO_IMPLICIT_NAMES_CAP;
	ClientCapabilityAdd(modinfo->handle, &cap, &CAP_NO_IMPLICIT_NAMES);
	*/

	return MOD_SUCCESS;
}

MOD_LOAD()
{
	return MOD_SUCCESS;
}

MOD_UNLOAD()
{
	return MOD_SUCCESS;
}

