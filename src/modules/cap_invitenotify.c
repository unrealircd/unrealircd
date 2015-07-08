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


#include "config.h"
#include "struct.h"
#include "common.h"
#include "sys.h"
#include "numeric.h"
#include "msg.h"
#include "proto.h"
#include "channel.h"
#include <time.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <io.h>
#endif
#include <fcntl.h>
#include "h.h"
#ifdef _WIN32
#include "version.h"
#endif
#include "m_cap.h"

ModuleHeader MOD_HEADER(cap_invitenotify)
  = {
        "cap_invitenotify",
        "$Id$",
        "invite-notify client capability", 
        "3.2-b8-1",
        NULL 
    };


static void cap_invitenotify_caplist(struct list_head *head)
{
ClientCapability *cap;

	cap = MyMallocEx(sizeof(ClientCapability));
	cap->name = strdup("invite-notify");
	cap->cap = PROTO_INVITENOTIFY;
	clicap_append(head, cap);
}

static void cap_invitenotify_invite(aClient *from, aClient *to, aChannel *chptr)
{
	sendto_channel_butone_with_capability(from, PROTO_INVITENOTIFY,
		from, chptr, "INVITE %s :%s", to->name, chptr->chname);
}

/* This is called on module init, before Server Ready */
DLLFUNC int MOD_INIT(cap_invitenotify)(ModuleInfo *modinfo)
{
        MARK_AS_OFFICIAL_MODULE(modinfo);

	HookAddVoid(modinfo->handle, HOOKTYPE_INVITE, 0, cap_invitenotify_invite);
	HookAddVoid(modinfo->handle, HOOKTYPE_CAPLIST, 0, cap_invitenotify_caplist);

        return MOD_SUCCESS;
}

/* Is first run when server is 100% ready */
DLLFUNC int MOD_LOAD(cap_invitenotify)(int module_load)
{
        return MOD_SUCCESS;
}

/* Called when module is unloaded */
DLLFUNC int MOD_UNLOAD(cap_invitenotify)(int module_unload)
{
        return MOD_SUCCESS;
}

