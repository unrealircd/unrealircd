/*
 * Privacy - hide channels in /WHOIS (User mode +p)
 * (C) Copyright 2000-.. Bram Matthys (Syzop) and the UnrealIRCd team
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 1, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "unrealircd.h"

#define IsPrivacy(cptr)    (cptr->umodes & UMODE_PRIVACY)

/* Module header */
ModuleHeader MOD_HEADER(privacy)
  = {
	"usermodes/privacy",
	"$Id$",
	"User Mode +p",
	"3.2-b8-1",
	NULL 
    };

/* Global variables */
long UMODE_PRIVACY = 0L;

/* Forward declarations */
int privacy_see_channel_in_whois(aClient *sptr, aClient *target, aChannel *chptr);
                    
DLLFUNC int MOD_INIT(privacy)(ModuleInfo *modinfo)
{
	UmodeAdd(modinfo->handle, 'p', UMODE_GLOBAL, umode_allow_all, &UMODE_PRIVACY);
	
	HookAddEx(modinfo->handle, HOOKTYPE_SEE_CHANNEL_IN_WHOIS, privacy_see_channel_in_whois);
	
	MARK_AS_OFFICIAL_MODULE(modinfo);
	return MOD_SUCCESS;
}

DLLFUNC int MOD_LOAD(privacy)(int module_load)
{
	return MOD_SUCCESS;
}

DLLFUNC int MOD_UNLOAD(privacy)(int module_unload)
{
	return MOD_SUCCESS;
}

/* This hides channels in /WHOIS output, unless the requestor is in the same channel
 * or some IRCOp is overriding.
 */
int privacy_see_channel_in_whois(aClient *sptr, aClient *target, aChannel *chptr)
{
	if (!IsMember(sptr, chptr))
		return EX_DENY;
	
	return EX_ALLOW;
}
