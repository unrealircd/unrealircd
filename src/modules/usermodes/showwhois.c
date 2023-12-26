/* 
 * IRC - Internet Relay Chat, src/modules/usermodes/showwhois.c
 * Show when someone does a /WHOIS on you (User mode +W)
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

#define IsWhois(cptr)    (cptr->umodes & UMODE_SHOWWHOIS)

/* Module header */
ModuleHeader MOD_HEADER
  = {
	"usermodes/showwhois",
	"4.2",
	"User Mode +W",
	"UnrealIRCd Team",
	"unrealircd-6",
    };

/* Global variables */
long UMODE_SHOWWHOIS = 0L;

/* Forward declarations */
int showwhois_whois(Client *requester, Client *target, NameValuePrioList **list);

MOD_TEST()
{
	return MOD_SUCCESS;
}

MOD_INIT()
{
	UmodeAdd(modinfo->handle, 'W', UMODE_GLOBAL, 1, umode_allow_opers, &UMODE_SHOWWHOIS);
	
	HookAdd(modinfo->handle, HOOKTYPE_WHOIS, 0, showwhois_whois);
	
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

int showwhois_whois(Client *requester, Client *target, NameValuePrioList **list)
{
	if (IsWhois(target) && (requester != target))
	{
		sendnotice(target,
			"*** %s (%s@%s) did a /whois on you.",
			requester->name,
			requester->user->username, requester->user->realhost);
	}

	return 0;
}
