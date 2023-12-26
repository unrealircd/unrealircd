/* 
 * IRC - Internet Relay Chat, src/modules/usermodes/privacy.c
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

#define IsPrivacy(client)    (client->umodes & UMODE_PRIVACY)

/* Module header */
ModuleHeader MOD_HEADER
  = {
	"usermodes/privacy",
	"4.2",
	"User Mode +p",
	"UnrealIRCd Team",
	"unrealircd-6",
    };

/* Global variables */
long UMODE_PRIVACY = 0L;

/* Forward declarations */
int privacy_see_channel_in_whois(Client *client, Client *target, Channel *channel);
                    
MOD_INIT()
{
	UmodeAdd(modinfo->handle, 'p', UMODE_GLOBAL, 0, umode_allow_all, &UMODE_PRIVACY);
	
	HookAdd(modinfo->handle, HOOKTYPE_SEE_CHANNEL_IN_WHOIS, 0, privacy_see_channel_in_whois);
	
	MARK_AS_OFFICIAL_MODULE(modinfo);
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

/* This hides channels in /WHOIS output, unless the requestor is in the same channel
 * or some IRCOp is overriding.
 */
int privacy_see_channel_in_whois(Client *client, Client *target, Channel *channel)
{
	if (IsPrivacy(target) && !IsMember(client, channel))
		return EX_DENY;
	
	return EX_ALLOW;
}
