/*
 *   IRC - Internet Relay Chat, src/modules/standard-replies.c
 *   (C) 2023 Syzop & The UnrealIRCd Team
 *
 *   See file AUTHORS in IRC package for additional names of
 *   the programmers.
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
	"standard-replies",
	"6.0",
	"standard-replies CAP", 
	"UnrealIRCd Team",
	"unrealircd-6",
	};

/* Variables */
long CAP_STANDARD_REPLIES = 0L;

MOD_INIT()
{
	ClientCapabilityInfo cap;

	MARK_AS_OFFICIAL_MODULE(modinfo);

	memset(&cap, 0, sizeof(cap));
	cap.name = "standard-replies";
	ClientCapabilityAdd(modinfo->handle, &cap, &CAP_STANDARD_REPLIES);

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
