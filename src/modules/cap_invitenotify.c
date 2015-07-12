/*
 * UnrealIRCd, src/modules/cap_invitenotify.c
 * Copyright (c) 2013 William Pitcock <nenolod@dereferenced.org>
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

ModuleHeader MOD_HEADER(cap_invitenotify)
  = {
        "cap_invitenotify",
        "$Id$",
        "invite-notify client capability", 
        "3.2-b8-1",
        NULL 
    };


static void cap_invitenotify_invite(aClient *from, aClient *to, aChannel *chptr)
{
	sendto_channel_butone_with_capability(from, PROTO_INVITENOTIFY,
		from, chptr, "INVITE %s :%s", to->name, chptr->chname);
}

MOD_INIT(cap_invitenotify)
{
	ClientCapability cap;
	MARK_AS_OFFICIAL_MODULE(modinfo);

	HookAddVoid(modinfo->handle, HOOKTYPE_INVITE, 0, cap_invitenotify_invite);

	memset(&cap, 0, sizeof(cap));
	cap.name = "invite-notify";
	cap.cap = PROTO_INVITENOTIFY;
	ClientCapabilityAdd(modinfo->handle, &cap);

	return MOD_SUCCESS;
}

MOD_LOAD(cap_invitenotify)
{
        return MOD_SUCCESS;
}

MOD_UNLOAD(cap_invitenotify)
{
        return MOD_SUCCESS;
}

