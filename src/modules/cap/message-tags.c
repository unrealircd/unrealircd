/*
 *   IRC - Internet Relay Chat, src/modules/message-tags.c
 *   (C) 2019 Syzop & The UnrealIRCd Team
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

ModuleHeader MOD_HEADER(message-tags)
  = {
	"message-tags",
	"4.2",
	"Message tags CAP", 
	"3.2-b8-1",
	NULL 
	};

long CAP_MESSAGE_TAGS = 0L;

MOD_INIT(message-tags)
{
	ClientCapabilityInfo cap;

	MARK_AS_OFFICIAL_MODULE(modinfo);

	memset(&cap, 0, sizeof(cap));
	cap.name = "message-tags";
	ClientCapabilityAdd(modinfo->handle, &cap, &CAP_MESSAGE_TAGS);
	return MOD_SUCCESS;
}

MOD_LOAD(message-tags)
{
	return MOD_SUCCESS;
}

MOD_UNLOAD(message-tags)
{
	return MOD_SUCCESS;
}
